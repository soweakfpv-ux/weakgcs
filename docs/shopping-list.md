# Lista zakupów

Wersja do odhaczania (arkusz): [shopping-list.xlsx](shopping-list.xlsx).

Zakładam, że masz już: **radio Radiomaster (EdgeTX)**, **drona z odbiornikiem ELRS 2,4 GHz i analogowym
VTX 5,8 GHz** oraz **laptopa**. Ceny są orientacyjne (PLN, 2026) i mocno zależą od sklepu.

## Wariant zasilania i okablowania

Lista zakłada najprostszy i najbezpieczniejszy wariant: **cztery pary skrętki rozdzielone ręcznie**
przejściówkami RJ45 ↔ zaciski śrubowe na obu końcach kabla:

| Para | Do czego |
|---|---|
| pomarańczowa + zielona | Ethernet (krótki patchcord do ESP32 i do laptopa) |
| niebieska | wideo analogowe (baluny) |
| brązowa | zasilanie 24 V |

Gotowe injectory pasywnego PoE kładą zasilanie na **obu** wolnych parach (niebieskiej i brązowej),
więc kolidują z wideo. **Nie podawaj też 24 V do gniazda RJ45 płytki ESP32**: do płytki idą tylko
pary danych.

## 1. Maszt: elektronika

| # | Co | Ile | ~Cena | Uwagi |
|---|---|---|---|---|
| 1 | **Olimex ESP32-POE** | 1 | 100–130 zł | Ethernet + USB do wgrywania firmware. Funkcja PoE w tym wariancie nieużywana; zasilanie 5 V z przetwornicy. Tańsza alternatywa: WT32-ETH01 (40–60 zł), ale wymaga przejściówki USB-UART do programowania (poz. 30). |
| 2 | **Radiomaster Nomad Dual 1W Gemini Xrossband** (ELRS 2,4 GHz + 868 MHz) | 1 | ok. 400–500 zł | Zasilanie 6–16,8 V przez XT30. Firmware EU/LBT i EU868; moc w UE: 2,4 GHz do 100 mW, 868 MHz do 25 mW. Ma 2 anteny dwuzakresowe. |
| 3 | ~~Odbiornik wideo 5,8 GHz~~: **masz Skyzone SteadyView** | – | 0 zł | Patrz sekcja „SteadyView na maszcie” niżej. SteadyView ma wbudowane diversity (dwa wejścia antenowe). |
| 4 | Antena 5,8 GHz **kierunkowa (patch)** | 1 | 60–200 zł | Polaryzacja (RHCP/LHCP) **taka sama jak w dronie**. |
| 5 | Antena 5,8 GHz dookólna (drugie wejście SteadyView) | 1 | 40–120 zł | Jak wyżej. Patch + dookólna to typowy zestaw do SteadyView. |
| 6 | Antena 2,4 GHz do ELRS (dipol/T w zestawie; opcjonalnie patch/moxon) | 0–1 | 0–150 zł | Na start wystarczy antena z modułu. |
| 7 | **Przetwornica step-down 5 V** (np. MP1584 / Pololu D24V) | 2 | 15–60 zł | ESP32 oraz osobno SteadyView. Wejście musi znieść 24 V. |
| 8 | **Przetwornica step-down 12 V, ≥2 A** (np. XL4015) + wtyk XT30 z przewodem | 1 | 20–40 zł | Zasilanie Nomada (6–16,8 V). |
| 9 | Kondensator low-ESR 470–1000 µF / 35 V | 2 | 5 zł | Na wejściu modułu ELRS (impulsowy pobór prądu). |
| 10 | Rezystor 1 kΩ | kilka | 1 zł | Linia CRSF: ESP32 TX → 1 kΩ → moduł (patrz [hardware.md](hardware.md)). |
| 11 | Konwerter poziomów logicznych (BSS138) | 1 | 5–10 zł | Tylko jeśli linia CRSF modułu jest 5 V. Kup na zapas. |
| 12 | Bezpiecznik / polyfuse ~2 A + oprawka | 1 | 5–15 zł | Na wejściu 24 V na maszcie. |
| 13 | Złącze do modułu ELRS (gniazdo JR / goldpiny / przewód z modułu) | 1 | 5–20 zł | Zależy od modułu. |

## 2. Wideo przez skrętkę

| # | Co | Ile | ~Cena | Uwagi |
|---|---|---|---|---|
| 14 | **Para pasywnych balunów wideo CCTV** (BNC ↔ skrętka, zaciski) | 1 para | 20–50 zł | Wersje z zaciskami śrubowymi są najwygodniejsze. |
| 15 | Przejściówka BNC (gniazdo) ↔ RCA (wtyk) | 1 | 5–10 zł | Przy laptopie: balun (BNC) → wejście RCA grabbera. Na maszcie niepotrzebna, bo wyjście SteadyView lutujesz do gniazda BNC. |
| 16 | **RdestGrabber** (rdest.io): grabber USB CVBS „no blue screen” | 1 | 159 zł | Wejście RCA, USB 2.0 UVC bez sterowników, zasilanie z USB. Przy słabym sygnale pokazuje zakłócony obraz zamiast niebieskiego ekranu. Bez sygnału po 2 s wyświetla logo, przy starcie przez 3 s ekran startowy. Formatu pikseli (YUYV/MJPEG) opis nie podaje; sprawdź `v4l2-ctl --list-formats-ext`. |

## 3. Kabel i sieć

| # | Co | Ile | ~Cena | Uwagi |
|---|---|---|---|---|
| 17 | **Skrętka zewnętrzna kat. 5e/6** (żelowana, odporna na UV), miedź, **nie CCA** | 30–100 m | 2–4 zł/m | CCA (aluminium miedziowane) ma dużo większą rezystancję, co oznacza duży spadek napięcia. |
| 18 | Bęben/zwijadło na kabel | 1 | 50–150 zł | Bardzo ułatwia rozkładanie w terenie. |
| 19 | **Przejściówka RJ45 ↔ zaciski śrubowe** (keystone/terminal) | 2–4 | 10–20 zł/szt. | Rozdzielenie par na obu końcach. |
| 20 | Krótkie patchcordy RJ45 0,25–0,5 m | 2–3 | 5–10 zł/szt. | Od przejściówki do ESP32 i do laptopa. |
| 21 | Wtyki RJ45 + zaciskarka + tester kabli | 1 kpl. | 50–120 zł | Jeśli zarabiasz kabel sam. |
| 22 | Karta sieciowa USB-Ethernet (np. RTL8153) | 0–1 | 40–80 zł | Tylko jeśli laptop nie ma gniazda Ethernet. |

## 4. Zasilanie (strona laptopa)

| # | Co | Ile | ~Cena | Uwagi |
|---|---|---|---|---|
| 23 | **Źródło 24 V, ≥2 A**: zasilacz 24 V **albo** akumulator LiPo 6S **albo** powerbank USB-C PD + wyzwalacz PD 20 V | 1 | 40–300 zł | W terenie: LiPo 6S (22,2–25,2 V) lub powerbank PD. Powerbank musi dawać 20 V przy ≥3 A. |
| 24 | Złącza (XT60, gniazdo DC 5,5/2,1) + przewody | – | 20 zł | |

## 5. Mechanika masztu

| # | Co | Ile | ~Cena | Uwagi |
|---|---|---|---|---|
| 25 | **Maszt teleskopowy 5–8 m** (włókno szklane lub aluminium; np. maszt antenowy/krótkofalarski lub mocny bat wędkarski) | 1 | 100–400 zł | Włókno szklane nie ekranuje anten. |
| 26 | Podstawa/statyw lub szpilka do wbicia w ziemię + **odciągi** | 1 kpl. | 50–200 zł | Wiatr na 6 m jest dużo silniejszy niż przy ziemi. |
| 27 | **Obudowa z druku 3D** ([hardware/enclosure](../hardware/enclosure/README.md)): filament ASA/PETG ok. 300 g, wtopki M3 ×11, wkręty M3×8 ×11, sznur uszczelniający Ø2 mm 0,6 m, dławik PG9 | 1 kpl. | 80–150 zł | Zamiast gotowej puszki IP65. |
| 28 | Przedłużka SMA **bulkhead** (gniazdo panelowe ↔ wtyk), RG316, 10–15 cm | 4 | 15–30 zł/szt. | 2× Nomad, 2× SteadyView (dookólna + patch w pokrywie). **Sprawdź SMA vs RP-SMA** w modułach i antenach. |
| 29 | Uchwyt anten / obejmy na rurę, opaski, rzepy | – | 30 zł | |

## 6. Warsztat (jeśli nie masz)

| # | Co | ~Cena | Uwagi |
|---|---|---|---|
| 30 | Przejściówka USB-UART 3,3 V (CP2102/CH340) | 15 zł | Tylko dla WT32-ETH01. |
| 31 | Multimetr | 50–150 zł | Pomiar spadku napięcia na kablu, poziomów logicznych. |
| 32 | Lutownica, cyna, koszulki termokurczliwe, przewody silikonowe | – | |
| 33 | Kabel USB-C do radia (dane, nie tylko ładowanie) | 20 zł | Radio jako joystick. |

## Podsumowanie kosztów (orientacyjnie)

| Grupa | ~Koszt |
|---|---|
| Maszt: elektronika (bez opcji) | 400–900 zł |
| Wideo przez skrętkę | 100–250 zł |
| Kabel i sieć (50 m) | 200–450 zł |
| Zasilanie | 60–300 zł |
| Mechanika | 250–800 zł |
| **Razem** | **ok. 1000–2700 zł** |

Najwięcej oszczędzisz, jeśli masz już moduł ELRS i zasilanie (LiPo 6S). Najwięcej zależy od masztu.

## Kolejność zakupów

Kupuj etapami, zgodnie z [bench-test.md](bench-test.md):

1. **Etap 1, stół (sterowanie):** ESP32-POE (1), moduł ELRS (2, jeśli nie masz), rezystor (10),
   kondensator (9), przetwornica 5 V (7), patchcord (20). Wystarczy do testu laptop → ESP32 → ELRS → dron.
2. **Etap 2, wideo:** przejście z SteadyView na AV (sekcja niżej), antena (4/5), grabber (16),
   baluny (14), przejściówki (15). Na początku zmierz opóźnienie bez balunów i bez długiego kabla.
3. **Etap 3, teren:** skrętka (17), bęben (18), przejściówki RJ45 (19), źródło 24 V (23), bezpiecznik (12),
   maszt (25–29).

## SteadyView na maszcie

Skyzone sprzedaje dwa różne produkty pod nazwą SteadyView:

| Wersja | Jak poznać | Co dokupić, żeby mieć wyjście AV |
|---|---|---|
| **SteadyView X** (z ekranikiem i przyciskami) | Ma własny wyświetlacz do wyboru kanału | **Płytka ground station** od Skyzone (bywa w zestawie „Ground Station Kit”): zasilanie 2–6S, wyjście AV na jack 3,5 mm, kabel AV w zestawie. Kanał ustawiasz na ekraniku, raz, na ziemi. |
| **SteadyView / RapidMix**, starszy moduł goglowy (**ten masz**) | Bez ekranu, wkładany do zatoki gogli (5 V, ok. 380 mA) | Nic gotowego: lutujesz 6 przewodów (5 V, GND, wideo, CLK, DATA, CS). Kanał ustawia ESP32 przez SPI, sterowany z laptopa. Szczegóły w [hardware.md](hardware.md#steadyview-starszy-moduł-goglowy-na-maszcie). Dokup: gniazdo/wtyk RCA lub BNC do lutowania, kawałek przewodu ekranowanego, ewentualnie osobną przetwornicę 5 V lub filtr LC (dławik + kondensator) dla czystszego obrazu. |

Tor wideo:

```
SteadyView X:          jack 3,5 mm ──► RCA (żółty) ── RCA↔BNC ── balun ══ … ── grabber USB
SteadyView (starszy):  pin wideo ── przewód ekranowany ── BNC ── balun ══ niebieska para ══ balun ── BNC↔RCA ── grabber USB
```

- (SteadyView X) **Kable jack 3,5 mm → RCA nie mają jednego standardu**: to, który pin niesie wideo, zależy od
  producenta. Użyj kabla od Skyzone albo sprawdź multimetrem (ciągłość), który pin jacka to wideo,
  a który to masa.
- Moduł ELRS TX i SteadyView stoją na maszcie obok siebie: rozsuń anteny 2,4 GHz i 5,8 GHz jak najdalej.
