# weakgcs: naziemna stacja FPV z masztem

Stacja naziemna do latania FPV, w której **nadajnik ELRS i odbiornik wideo stoją wysoko na maszcie**,
a pilot siedzi przy laptopie połączonym z masztem jednym kablem (skrętką do 100 m).

```
 Radiomaster (EdgeTX, USB joystick)
        │ USB
        ▼
 ┌──────────────┐   skrętka (Ethernet + wideo analogowe + zasilanie)   ┌──────────────────────────┐
 │   LAPTOP     │══════════════════════════════════════════════════════│  MASZT                   │
 │  weakgcs     │   UDP: RC 500 Hz  ──►                                │  ESP32 + Ethernet        │
 │  - RC        │   ◄── status 10 Hz + telemetria ELRS                 │   └─ CRSF ─► moduł ELRS  │
 │  - wideo+OSD │   ◄── wideo analogowe (balun) ─► grabber USB         │  VRX 5.8 GHz ─► balun    │
 └──────────────┘                                                      └──────────────────────────┘
```

## Struktura repozytorium

| Katalog | Co to jest |
|---|---|
| `common/include/weakgcs/` | Wspólne nagłówki: protokół UDP laptop↔maszt (`protocol.h`) i CRSF (`crsf.h`). Używane przez firmware i aplikację. |
| `firmware/mast/` | Firmware ESP32 na maszt (PlatformIO): UDP → CRSF do modułu ELRS, telemetria z powrotem, failsafe. |
| `gcs/` | Aplikacja na laptopa (C++20 + SDL3): joystick → UDP, wideo z grabbera, OSD. |
| `tests/` | Testy jednostkowe (CRSF, protokół, mapowanie kanałów, config). |
| `tools/fake_mast.py` | Symulator masztu do testowania aplikacji bez sprzętu. |
| `docs/` | Sprzęt, protokół, budowanie, procedura testów. |

## Szybki start

```bash
# Aplikacja GCS (Ubuntu; Windows: patrz docs/gcs.md)
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/gcs/weakgcs            # czyta build/gcs/gcs.ini

# Test bez sprzętu: w gcs.ini ustaw mast_ip = 127.0.0.1, potem
python3 tools/fake_mast.py

# Firmware masztu
cd firmware/mast && pio run -e esp32-poe -t upload
```

Klawisze w aplikacji: **F11** pełny ekran, **F1** OSD wł./wył., **F2** monitor kanałów, **Esc** wyjście,
**Ctrl+←/→** kanał wideo, **Ctrl+↑/↓** pasmo wideo (gdy `vrx_channel` jest ustawione).

## Najważniejsze założenia

- **Radio robi miksowanie.** Radiomaster w trybie USB Joystick (EdgeTX) wystawia kanały modelu jako osie
  i przyciski HID. Aplikacja przekazuje je 1:1 do CRSF, więc model, przełączniki i expo zostają w EdgeTX.
- **Niskie opóźnienie sterowania.** Joystick jest próbkowany w osobnym wątku czasu rzeczywistego tuż przed
  każdym pakietem, niezależnie od renderowania. ESP32 nadaje CRSF z timera sprzętowego, w rytmie, który
  dyktuje moduł ELRS (synchronizacja jak w EdgeTX).
- **Wideo bez kodowania.** Sygnał analogowy idzie z VRX skrętką (balun), a grabber USB podaje surowe klatki.
  Aplikacja zawsze pokazuje najnowszą klatkę i domyślnie wyłącza VSync.
- **Bezpieczeństwo:**
  - brak pakietów z laptopa przez 200 ms → maszt przestaje nadawać CRSF → dron wchodzi w failsafe;
  - odłączenie radia od USB → aplikacja przestaje wysyłać, efekt jak wyżej;
  - **arm guard:** po starcie i po ponownym podłączeniu radia kanał ARM jest trzymany nisko, dopóki
    przełącznik na radiu nie zostanie zobaczony w pozycji „rozbrojony”.

## Windows czy Ubuntu?

Działa na obu. **Ubuntu jest lepsze pod opóźnienia**: wątek RC może dostać `SCHED_FIFO` (prawdziwy
priorytet czasu rzeczywistego), grabber działa przez V4L2 bez warstw pośrednich, w tle jest mniej rzeczy,
które wybudzają CPU. Windows też jest w porządku (wysoki priorytet wątku, timery 1 ms / high-resolution),
ale jitter jest większy. Szczegóły w [docs/gcs.md](docs/gcs.md).

## Dokumentacja

- [docs/shopping-list.md](docs/shopping-list.md): lista zakupów z cenami i kolejnością
- [docs/hardware.md](docs/hardware.md): lista części, okablowanie skrętki, podłączenie ESP32 ↔ ELRS, zasilanie
- [docs/gcs.md](docs/gcs.md): budowanie i konfiguracja aplikacji (Windows / Ubuntu), EdgeTX, strojenie opóźnień
- [docs/protocol.md](docs/protocol.md): protokół UDP laptop↔maszt i użyte ramki CRSF
- [docs/bench-test.md](docs/bench-test.md): **procedura testów na stole przed pierwszym lotem**

## Status

Szkielet projektu: kod się kompiluje, testy przechodzą, aplikacja współpracuje z symulatorem masztu.
**Nic nie było jeszcze testowane na prawdziwym sprzęcie** (ESP32, moduł ELRS, grabber). Zacznij od
[docs/bench-test.md](docs/bench-test.md), bez śmigieł.
