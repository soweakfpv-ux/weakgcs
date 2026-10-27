# Sprzęt

Pełna lista zakupów z cenami: [shopping-list.md](shopping-list.md).

## Lista części

| Element | Propozycja | Uwagi |
|---|---|---|
| Kontroler masztu | **Olimex ESP32-POE** | Ethernet + USB do programowania, zasilany 5 V z przetwornicy. Alternatywa: WT32-ETH01. |
| Moduł ELRS TX | **Radiomaster Nomad Dual 1W Gemini Xrossband** (2,4 GHz + 868 MHz, 80×45×40 mm) | Zasilanie 6–16,8 V przez XT30 (przetwornica 12 V). Sygnał CRSF z pinów zatoki modułu. |
| VRX | Analogowy odbiornik 5.8 GHz z wyjściem AV (np. RX5808 z diversity, albo VRX z wyjściem AV) | Na maszcie najlepiej z anteną kierunkową (patch/helical) i dookólną w diversity. |
| Baluny wideo | Para pasywnych balunów CCTV (BNC ↔ skrętka) | Wideo analogowe na jednej parze skrętki, zero opóźnienia, do kilkuset metrów. |
| Grabber USB | Grabber AV → USB (UVC) | **Przetestuj kilka modeli**, różnią się opóźnieniem o dziesiątki ms. Szukaj takich, które podają YUYV (nieskompresowane). |
| Zasilanie | 24 V (zasilacz, LiPo 6S lub powerbank PD 20 V) na brązowej parze | Patrz „Okablowanie skrętki” niżej. |
| Przetwornice | Step-down 5 V (≥3 A) i ewentualnie 12 V dla VRX | Zależnie od tego, czym zasilane są moduł i VRX. |
| Kabel | Skrętka kat. 5e/6, zewnętrzna (żelowana/UV), do 100 m | |
| Maszt | Teleskopowy maszt wędkarski/antenowy 4–8 m | Elektronika w szczelnej puszce, anteny na górze. |

## Okablowanie skrętki

Ethernet 100 Mbit/s używa tylko dwóch par (pomarańczowej i zielonej). Zostają dwie wolne:

| Para | Przeznaczenie |
|---|---|
| pomarańczowa (1-2) | Ethernet TX |
| zielona (3-6) | Ethernet RX |
| niebieska (4-5) | **wideo analogowe przez balun** |
| brązowa (7-8) | zasilanie 24 V (+ i −) |

**Zalecany wariant:** pary rozdzielone ręcznie przejściówkami RJ45 ↔ zaciski śrubowe na obu końcach
kabla. Do ESP32 i laptopa idą krótkimi patchcordami tylko pary danych, niebieska trafia do balunów,
a brązowa niesie 24 V do przetwornic na maszcie.

Nie używaj gotowych injectorów PoE (ani pasywnych, ani 802.3af mode B): kładą zasilanie na obu wolnych
parach, czyli także na niebieskiej, którą idzie wideo. Z tego samego powodu **nie podawaj 24 V do gniazda
RJ45 płytki ESP32**. Płytkę zasilasz 5 V z przetwornicy.

**Spadek napięcia.** Żyła skrętki 24 AWG ma ok. 0,084 Ω/m. Jedna para (+ na jednej żyle, − na drugiej)
na 100 m daje pętlę 2 × 100 m × 0,084 Ω/m ≈ 17 Ω, czyli przy 0,6 A spadek ≈ 10 V. Z 24 V zostaje ok. 14 V,
co dla przetwornicy step-down nadal jest OK, ale **policz to dla swojej długości kabla i poboru**
(moduł ELRS 1 W + VRX + ESP32 to ok. 10–15 W). Krótszy kabel, wyższe napięcie (48 V) albo dwie pary na
zasilanie zmniejszają problem.

Alternatywa: osobny kabel albo akumulator na maszcie. Skrętka zostaje wtedy tylko na dane i wideo.

## Podłączenie ESP32 ↔ moduł ELRS

Moduł ELRS (zatoka JR / złącze modułu) ma jedną linię sygnałową CRSF (często opisaną jako S.Port),
half-duplex, nieodwróconą. ESP32 ma osobne TX i RX, więc łączymy je tak:

```
 ESP32 TX ──[ 1 kΩ ]──┬──── CRSF / S.Port modułu
 ESP32 RX ────────────┘
 GND ──────────────────────── GND modułu
                              VBAT modułu ◄── zasilanie (5–8 V, sprawdź moduł)
```

- Rezystor pozwala modułowi „przekrzyczeć” linię, gdy odsyła telemetrię. Własne ramki ESP32 odbiera
  jako echo i firmware je ignoruje.
- **Poziomy logiczne:** ESP32 to 3,3 V. Większość modułów ELRS ma linię CRSF 3,3 V lub toleruje 3,3 V,
  ale **sprawdź swój moduł**. Jeśli linia jest 5 V, daj konwerter poziomów.

| Płytka | CRSF TX | CRSF RX |
|---|---|---|
| Olimex ESP32-POE | GPIO4 | GPIO36 |
| WT32-ETH01 | GPIO17 | GPIO5 |

Piny ustawiasz w `firmware/mast/src/config.h`.

## SteadyView (starszy moduł goglowy) na maszcie

Dotyczy wersji **Skyzone SteadyView V3.3** (płytka SteadyView na płytce bazowej RX5G, dwa złącza
SMA, listwa pinów do zatoki gogli wzdłuż dłuższej krawędzi, USB-C do aktualizacji firmware).
Moduł jest zrobiony do zatoki gogli Skyzone, więc nie ma własnego gniazda AV ani wyboru kanału.
W goglach kanał ustawia się przez 3 linie SPI. Na maszcie tę rolę przejmuje ESP32, więc
**kanał wideo zmieniasz z laptopa** (`vrx_channel` w `gcs.ini`, Ctrl+strzałki w aplikacji).

Do modułu podłączasz 6 przewodów, lutując je do jego złącza (albo wkładając w gniazdo goldpin
żeńskie, jeśli raster pasuje):

| Sygnał modułu | Dokąd | Uwagi |
|---|---|---|
| **5 V** | przetwornica 5 V | ok. 380 mA, dobrze dać osobną przetwornicę albo filtr LC (szumy z przetwornicy widać na obrazie) |
| **GND** | masa | wspólna z ESP32 |
| **Video** | balun (przez wtyk RCA/BNC) | sygnał kompozytowy ok. 1 Vpp, przewód ekranowany lub możliwie krótki |
| **CLK** (pin 1 SPI) | ESP32 `VRX_CLK_PIN` | 3,3 V |
| **DATA** (pin 2 SPI) | ESP32 `VRX_DATA_PIN` | 3,3 V |
| **CS** (pin 3 SPI) | ESP32 `VRX_CS_PIN` | 3,3 V |

| Płytka | CLK | DATA | CS |
|---|---|---|---|
| Olimex ESP32-POE | GPIO32 | GPIO33 | GPIO13 |
| WT32-ETH01 | GPIO14 | GPIO4 | GPIO33 |

**Zanim cokolwiek podłączysz:** dokładny układ pinów złącza modułu nie jest tu potwierdzony.
Oznaczenie CLK/DATA/CS jako pinów 1/2/3 pochodzi z dokumentacji ExpressLRS VRX Backpack. Pozostałe
piny (5 V, GND, wideo) ustal tak:

1. Obejrzyj opisy na płytce modułu (często są nadrukowane przy pinach).
2. GND: ciągłość z metalowym ekranem/obudową modułu.
3. 5 V: w goglach, na włączonych goglach, zmierz napięcie na pinach zatoki względem GND.
4. Wideo: pin, który w goglach idzie do wejścia wideo. W razie wątpliwości oscyloskop
   (sygnał ok. 1 Vpp z impulsami synchronizacji co 64 µs).

Sterowanie SPI: przy pierwszym ustawieniu kanału firmware przełącza moduł w tryb **Mix**
(długi impuls na CLK, tak jak robi to ELRS Backpack), potem wpisuje częstotliwość do rejestrów
RTC6705. Aplikacja ponawia żądanie co 250 ms, więc po restarcie masztu kanał ustawi się sam.

## Wideo

```
VRX (wyjście AV) ── BNC ── balun ══ niebieska para ══ balun ── BNC/RCA ── grabber USB ── laptop
```

- Masa VRX i masa zasilania na maszcie: uważaj na pętle masy, bo dają paski na obrazie. Pasywne baluny
  izolują galwanicznie, co zwykle pomaga.
- Opóźnienie toru wideo w tym układzie to praktycznie tylko grabber i ekran laptopa. Mierz je
  (procedura w [bench-test.md](bench-test.md)).

## Kwestie prawne (PL/UE), w skrócie

- Lot FPV (z gogli albo z ekranu laptopa) wymaga w kategorii otwartej obserwatora w zasięgu wzroku drona.
- Limity mocy: 5,8 GHz wideo do 25 mW EIRP, ELRS 2,4 GHz do 100 mW EIRP (868 MHz ma osobne limity).
  Wysoki maszt daje zasięg dzięki wysokości, bez zwiększania mocy.
