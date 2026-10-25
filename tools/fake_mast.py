#!/usr/bin/env python3
"""Mast simulator: lets you test the GCS app without any hardware.

Listens like the real mast, prints the received RC channels, and sends back mast status plus
fake ELRS telemetry (link stats, battery, flight mode) so the OSD has something to show.

    python3 tools/fake_mast.py                 # listen on 0.0.0.0:5770
    then in gcs.ini: mast_ip = 127.0.0.1

Stop sending from the GCS (unplug the radio) and it reports FAILSAFE after 200 ms, like the mast.
"""
import argparse
import math
import socket
import struct
import time

MAGIC = 0x53434757
VERSION = 1
MSG_RC, MSG_TELEMETRY, MSG_MAST_STATUS = 1, 2, 3
WAITING, ACTIVE, FAILSAFE = 0, 1, 2
FAILSAFE_TIMEOUT = 0.2

HEADER = struct.Struct("<IBBH")
RC = struct.Struct("<IBBHIBB16HH")  # header, gcsTimeUs, flags, reserved, channels, crc
STATUS_BODY = struct.Struct("<IBBHII6IIi")


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0xD5) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def seal(msg_type: int, seq: int, body: bytes) -> bytes:
    data = HEADER.pack(MAGIC, VERSION, msg_type, seq & 0xFFFF) + body
    return data + struct.pack("<H", crc16(data))


def crsf_frame(frame_type: int, payload: bytes) -> bytes:
    body = bytes([frame_type]) + payload
    return bytes([0xEA, len(body) + 1]) + body + bytes([crc8(body)])


def telemetry_packet(seq: int, frame: bytes) -> bytes:
    return seal(MSG_TELEMETRY, seq, bytes([len(frame)]) + frame.ljust(64, b"\0"))


def ticks_to_us(t: int) -> int:
    return (t - 992) * 5 // 8 + 1500


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=5770)
    ap.add_argument("--quiet", action="store_true", help="don't print channels")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    sock.settimeout(0.01)
    print(f"fake mast listening on {args.bind}:{args.port}")

    start = time.monotonic()
    gcs = None
    state, last_rc, last_seq, last_gcs_time = WAITING, 0.0, 0, 0
    rc_ok = rc_bad = failsafes = 0
    tx_seq = 0
    next_status = next_telem = next_print = 0.0
    channels = [992] * 16

    while True:
        try:
            data, addr = sock.recvfrom(512)
        except socket.timeout:
            data = None
        now = time.monotonic()

        if data:
            if len(data) == RC.size and crc16(data[:-2]) == struct.unpack_from("<H", data, RC.size - 2)[0]:
                f = RC.unpack(data)
                if f[0] == MAGIC and f[2] == MSG_RC:
                    last_seq, last_gcs_time, channels = f[3], f[4], list(f[7:23])
                    last_rc, gcs = now, addr
                    rc_ok += 1
                else:
                    rc_bad += 1
            else:
                rc_bad += 1

        if gcs is None:
            continue
        if now - last_rc > FAILSAFE_TIMEOUT:
            if state == ACTIVE:
                failsafes += 1
                print("FAILSAFE: no RC from GCS")
            state = FAILSAFE
        else:
            if state != ACTIVE:
                print("ACTIVE")
            state = ACTIVE

        if now >= next_status:
            next_status = now + 0.1
            body = STATUS_BODY.pack(
                int((now - start) * 1000) & 0xFFFFFFFF, state, 0, last_seq, last_gcs_time,
                int((now - last_rc) * 1e6) & 0xFFFFFFFF, rc_ok, rc_bad, failsafes, rc_ok, 0, 0,
                20000, 0)  # pretend ELRS runs at 500 Hz
            sock.sendto(seal(MSG_MAST_STATUS, tx_seq, body), gcs)
            tx_seq += 1

        if state == ACTIVE and now >= next_telem:
            next_telem = now + 0.2
            t = now - start
            lq = int(85 + 15 * math.sin(t / 5))
            frames = [
                crsf_frame(0x14, struct.pack("<BBBbBBBBBb", 48, 52, lq, 9, 0, 7, 3, 55, 100, 8)),
                crsf_frame(0x08, struct.pack(">HH", int(165 - t % 30), 123) + (int(t * 3)).to_bytes(3, "big") + bytes([80])),
                crsf_frame(0x21, b"ACRO\0"),
            ]
            for fr in frames:
                sock.sendto(telemetry_packet(tx_seq, fr), gcs)
                tx_seq += 1

        if not args.quiet and now >= next_print:
            next_print = now + 0.5
            us = " ".join(f"{ticks_to_us(c):4d}" for c in channels[:8])
            print(f"rc {rc_ok:7d} ok {rc_bad:3d} bad  ch1-8: {us}")


if __name__ == "__main__":
    main()
