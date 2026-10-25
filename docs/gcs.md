# Aplikacja GCS (laptop)

C++20 + [SDL3](https://libsdl.org). SDL3 obsługuje joystick, przechwytywanie wideo (V4L2 na Linuksie,
Media Foundation na Windows) i renderowanie, więc jest jedyną zależnością. Jeśli SDL3 nie jest
zainstalowane w systemie, CMake pobiera je i linkuje statycznie (wersja przypięta w `gcs/CMakeLists.txt`).

## Budowanie: Ubuntu (zalecane)

```bash
sudo apt install build-essential cmake ninja-build git pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
    libxkbcommon-dev libwayland-dev libdecor-0-dev libdrm-dev libgbm-dev \
    libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev libpipewire-0.3-dev

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/gcs/weakgcs
```

Bez pakietów X11/Wayland SDL też się zbuduje, ale okno nie będzie miało na czym się otworzyć.

### Priorytet czasu rzeczywistego (Linux)

Wątek RC prosi o `SCHED_FIFO`. Zwykły użytkownik domyślnie go nie dostanie (OSD pokaże „no RT prio”).
Jedna z opcji:

```bash
# jednorazowo dla pliku wykonywalnego
sudo setcap cap_sys_nice+ep build/gcs/weakgcs

# albo na stałe dla użytkownika: /etc/security/limits.d/99-weakgcs.conf
#   twoj_user  -  rtprio  60
# (wyloguj się i zaloguj ponownie)
```

### Dostęp do urządzeń

- Radio jako joystick: `/dev/input/event*`, zwykle dostępne dla zalogowanego użytkownika.
- Grabber: `/dev/video*`. Użytkownik powinien być w grupie `video` (`sudo usermod -aG video $USER`).

## Budowanie: Windows

Visual Studio 2022 (workload „Desktop development with C++”, zawiera CMake i Ninja):

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
.\build\gcs\Release\weakgcs.exe
```

Na Windows aplikacja sama włącza timer 1 ms, high-resolution waitable timer i priorytet
`TIME_CRITICAL` dla wątku RC. Warto też:

- ustawić plan zasilania „Najwyższa wydajność” i wyłączyć oszczędzanie energii USB
  (selective suspend), bo potrafi usypiać radio i grabber;
- **zezwolić aplikacji na dostęp do kamery** (Ustawienia → Prywatność → Kamera → aplikacje klasyczne);
- przy pierwszym uruchomieniu zezwolić w zaporze na ruch w sieci prywatnej (UDP 5770).

## Sieć

Laptop i maszt łączą się bezpośrednio kablem (bez routera):

| | IP |
|---|---|
| Laptop (port Ethernet) | `192.168.77.1/24`, bez bramy |
| Maszt | `192.168.77.2` (w firmware, `config.h`) |

Ubuntu: `nmcli con add type ethernet ifname <eth> con-name mast ipv4.method manual ipv4.addresses 192.168.77.1/24`.
Windows: właściwości karty → IPv4 → ręcznie.

## EdgeTX (Radiomaster jako joystick)

1. Podłącz radio kablem USB-C i wybierz **USB Joystick (HID)**.
2. W ustawieniach modelu (Model → USB Joystick) zostaw tryb **Classic**: kanały 1–8 trafiają na 8 osi.
   Tryb Advanced pozwala mapować więcej kanałów na przyciski.
3. Uruchom aplikację, naciśnij **F2** (monitor kanałów) i sprawdź, czy drążki i przełączniki ruszają
   właściwymi kanałami (AETR + AUX). Jeśli nie, popraw `channels` w `gcs.ini`, np.
   `channels = a0 a1 -a2 a3 a4 a5 a6 a7` odwraca kanał 3.
4. Kanał ARM (`arm_channel`, domyślnie 5 = AUX1) jest blokowany nisko, dopóki aplikacja nie zobaczy
   przełącznika w pozycji „rozbrojony”.

Uwaga: moduł ELRS w radiu (wewnętrzny lub zewnętrzny) możesz zostawić wyłączony. Radio służy tu
wyłącznie jako drążki.

## Konfiguracja

Plik `gcs.ini` jest kopiowany obok pliku wykonywalnego przy budowaniu. Można też podać ścieżkę jako
argument: `weakgcs moj.ini`. Opis wszystkich opcji jest w [gcs/config/gcs.ini](../gcs/config/gcs.ini).
Nieznane klucze są błędem, żeby literówka nie przeszła niezauważona.

## Gdzie powstaje opóźnienie (i co z tym zrobić)

| Etap | Typowo | Uwagi |
|---|---|---|
| Radio → USB HID | 1–8 ms | EdgeTX raportuje co ok. 1–4 ms, plus odpytywanie USB. |
| Laptop → maszt (UDP) | < 1 ms | Kabel bezpośredni. RTT widać w OSD. |
| ESP32 → moduł ELRS | 0–1 okres pakietu | Synchronizacja z modułem minimalizuje czekanie. |
| ELRS (np. 500 Hz) | ~2–4 ms | Zależy od trybu (Packet Rate) ustawionego w module. |
| **Grabber wideo** | **30–100 ms** | Największy składnik. Testuj grabbery, wybieraj formaty YUYV. |
| Ekran laptopa | 5–30 ms | VSync off (domyślnie), ekran 120 Hz+ pomaga. |

## Symulator masztu

`tools/fake_mast.py` udaje maszt: drukuje odebrane kanały, odsyła status i sztuczną telemetrię.
Ustaw `mast_ip = 127.0.0.1` i uruchom `python3 tools/fake_mast.py`.
