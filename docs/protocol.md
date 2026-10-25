# Protokół laptop ↔ maszt

Definicja: [`common/include/weakgcs/protocol.h`](../common/include/weakgcs/protocol.h). Ten sam plik
kompiluje się w aplikacji i w firmware, więc obie strony zawsze mają ten sam układ pakietów.

- Transport: UDP, maszt nasłuchuje na porcie **5770**.
- Maszt odpowiada na adres i port, z którego przyszedł ostatni pakiet RC, więc laptop nie potrzebuje
  stałego portu.
- Kolejność bajtów: little-endian. Struktury `#pragma pack(1)` są wysyłane bezpośrednio.
- Każdy pakiet: nagłówek `magic "WGCS"`, `version`, `type`, `seq`, a na końcu CRC-16/CCITT-FALSE
  liczone po wszystkich wcześniejszych bajtach.

| Typ | Kierunek | Częstotliwość | Zawartość |
|---|---|---|---|
| `MSG_RC` (1) | laptop → maszt | `rate_hz` (500) | 16 kanałów w tickach CRSF (172..1811), czas laptopa (do RTT), flagi |
| `MSG_TELEMETRY` (2) | maszt → laptop | każda ramka od modułu | Surowa ramka CRSF z modułu ELRS (link stats, bateria, GPS, tryb lotu…) |
| `MSG_MAST_STATUS` (3) | maszt → laptop | 10 Hz | Stan masztu, echo czasu laptopa, liczniki, timing ELRS |

## Logika masztu

```
WAITING ──(pierwszy poprawny RC)──► ACTIVE ──(200 ms bez RC)──► FAILSAFE
                                      ▲                            │
                                      └──────(RC wraca)────────────┘
```

- **ACTIVE:** w każdym takcie timera wysyła do modułu ramkę `RC_CHANNELS_PACKED` z ostatnimi kanałami.
- **FAILSAFE / WAITING:** nic nie wysyła na linię CRSF. Moduł przestaje dostawać kanały, link ELRS się
  zrywa i odbiornik w dronie przechodzi w swój failsafe (ustawiany w Betaflight/INAV).
- Pakiety starsze niż ostatni (po `seq`) są odrzucane. Wyjątek: przerwa > 500 ms, bo wtedy zakładamy
  restart aplikacji i licznik od zera.

## Synchronizacja z modułem ELRS

Moduł ELRS wysyła ramkę `RADIO_ID (0x3A)` z podtypem `0x10` („OpenTX sync”), w której podaje:

- `rate`: okres pakietów, który chce dostawać (w 0,1 µs, np. 20000 = 2 ms = 500 Hz),
- `offset`: o ile nasze ramki są przesunięte względem idealnego momentu.

Firmware ustawia okres timera na `rate` i stopniowo (maks. 10% okresu na takt) koryguje fazę o
`offset`, tak jak robi to EdgeTX. Dzięki temu kanały trafiają do modułu tuż przed wysłaniem pakietu
radiowego, a nie losowo w ramach okresu.

## RTT

`rtt = teraz − echoGcsTimeUs − echoAgeUs`. Maszt podaje, jak dawno dostał pakiet, którego czas odsyła,
więc RTT nie zawiera czasu oczekiwania na wysłanie statusu. OSD pokazuje RTT w ms.

## Ramki CRSF

[`common/include/weakgcs/crsf.h`](../common/include/weakgcs/crsf.h): budowanie ramek RC, parser
strumieniowy, dekodowanie `LINK_STATISTICS (0x14)`, `BATTERY (0x08)`, `GPS (0x02)`,
`ATTITUDE (0x1E)`, `FLIGHT_MODE (0x21)` i `RADIO_ID (0x3A)`.
