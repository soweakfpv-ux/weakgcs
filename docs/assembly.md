# Instrukcja montażu krok po kroku

Kolejność jest celowa: każdy etap kończy się testem, a następny zaczynasz dopiero, gdy poprzedni
działa. Dzięki temu szukasz błędu w jednym nowym elemencie, a nie w całym systemie naraz.

**Przez cały montaż i wszystkie testy dron jest bez śmigieł.**

Powiązane dokumenty: [hardware.md](hardware.md) (schematy i piny), [gcs.md](gcs.md) (aplikacja),
[bench-test.md](bench-test.md) (testy), [shopping-list.xlsx](shopping-list.xlsx) (części).

---

## Schemat całości

```
 STRONA LAPTOPA                                    SKRĘTKA                               MASZT (obudowa z druku 3D)
 ─────────────                                     ───────                               ──────────────────────────
 Radiomaster ──USB-C──┐                                                          ┌── bezpiecznik 2 A ──┬─ przetwornica A 5 V ─┬─ ESP32-POE (5 V)
                      ▼                                                          │                     ├─ przetwornica C 12 V ── XT30 ── Radiomaster Nomad (6–16,8 V)
 ┌────────┐  patchcord  ┌──────────┐  pomarańczowa + zielona (Ethernet)  ┌──────────┐ patchcord        └─ przetwornica B 5 V ── filtr LC ── SteadyView 5 V
 │ LAPTOP │─────────────│ RJ45 ↔   │═════════════════════════════════════│ RJ45 ↔   │──────────── ESP32-POE (RJ45)
 │        │             │ zaciski  │  brązowa (24 V)                     │ zaciski  │
 │        │  24 V ──────│          │═════════════════════════════════════│          │── 24 V ──────► bezpiecznik (wyżej)
 │        │             │          │  niebieska (wideo)                  │          │
 │        │  grabber ◄─ RCA ◄─ BNC ◄─ balun ◄═══════════════════════════════ balun ◄── BNC ◄── wyjście wideo SteadyView
 └────────┘   (USB)     └──────────┘                                     └──────────┘
                                                                         ESP32: GPIO4 ─[1 kΩ]─┬─ CRSF modułu ELRS     GPIO32/33/13 ── CLK/DATA/CS SteadyView
                                                                                GPIO36 ───────┘
 Moduł ELRS: Radiomaster Nomad (zasilanie przez XT30, sygnał CRSF przez piny zatoki modułu).
```

Piny podane są dla Olimex ESP32-POE. Dla WT32-ETH01 patrz [hardware.md](hardware.md).

---

## Krok 0. Przygotowanie

**Narzędzia:** lutownica z cienkim grotem, cyna z topnikiem, multimetr, ściągacz izolacji, zaciskarka
RJ45 i tester kabli, koszulki termokurczliwe, opalarka lub zapalniczka, śrubokręty płaskie 2–3 mm,
wiertarka (otwory w puszce).

**Zasady:**

- Lutuj i przełączaj przewody **bez zasilania**.
- Przetwornice ustawiasz **zanim** podłączysz do nich cokolwiek (krok 3).
- Każde nowe połączenie zasilające sprawdź multimetrem (ciągłość, brak zwarcia +/−) przed włączeniem.
- Zapisuj kolory przewodów, które przydzielasz sygnałom. Przydadzą się przy szukaniu usterek.

---

## Krok 1. Laptop: oprogramowanie i radio

1. Zbuduj aplikację według [gcs.md](gcs.md) (Ubuntu zalecane), uruchom testy: `ctest --test-dir build`.
2. Ustaw port Ethernet laptopa na stały adres **192.168.77.1/24**, bez bramy.
3. Podłącz Radiomaster kablem USB-C (z transmisją danych) i wybierz w radiu **USB Joystick (HID)**.
4. Test bez masztu: w `gcs.ini` ustaw `mast_ip = 127.0.0.1`, uruchom `python3 tools/fake_mast.py`
   i aplikację.
5. Naciśnij **F2** i sprawdź, czy każdy drążek i przełącznik rusza właściwym kanałem (AETR + AUX).
   Popraw `channels` w `gcs.ini`, jeśli trzeba.
6. Przywróć `mast_ip = 192.168.77.2`.

**Sprawdzenie:** OSD pokazuje `MAST ACTIVE` z symulatorem, a kanały reagują na drążki.

---

## Krok 2. ESP32: wgranie firmware

1. Podłącz Olimex ESP32-POE do laptopa przez USB. Na razie bez innych przewodów.
2. Wgraj firmware:
   ```bash
   cd firmware/mast
   pio run -e esp32-poe -t upload
   pio device monitor
   ```
3. W monitorze powinno pojawić się `weakgcs mast starting` i `[eth] static IP 192.168.77.2`.
4. Połącz ESP32 z laptopem zwykłym patchcordem. W logu pojawi się `[eth] link up`.
5. `ping 192.168.77.2` z laptopa musi odpowiadać.
6. Uruchom aplikację. Log masztu przechodzi w `ACTIVE rc=500/s`, a OSD pokazuje `MAST ACTIVE`
   i RTT poniżej 1 ms. Moduł ELRS nie jest jeszcze podłączony, więc `ELRS NO TELEMETRY` jest normalne.

**Sprawdzenie:** ping działa, OSD `MAST ACTIVE`. Po wyjęciu radia z USB log masztu pokazuje `FAILSAFE`.

---

## Krok 3. Zasilanie masztu (na stole)

Na tym etapie zasilasz wszystko z zasilacza 24 V bezpośrednio, bez skrętki.

1. **Bezpiecznik:** zamontuj oprawkę z bezpiecznikiem / polyfuse ~2 A na plusie wejścia 24 V.
2. **Przetwornica A (5 V, ESP32 + ELRS):**
   1. Podłącz samo wejście (24 V) bez żadnego obciążenia.
   2. Potencjometrem ustaw na wyjściu **5,0–5,1 V** (multimetr).
   3. Odłącz zasilanie.
3. **Przetwornica B (5 V, SteadyView):** tak samo, 5,0 V. Na jej wyjściu zlutuj filtr LC: dławik
   10–47 µH szeregowo w plusie, za nim kondensator ≥100 µF do masy.
4. Na wejściu zasilania modułu ELRS przylutuj kondensator low-ESR 470–1000 µF (uważaj na
   biegunowość: pasek = minus).
5. **Masa wspólna:** minus wejścia, masy obu przetwornic, GND ESP32, GND modułu ELRS i GND SteadyView
   łącz w jednym punkcie (gwiazda), a nie „łańcuszkiem”.
6. Zasil ESP32 z przetwornicy A przez pin **5 V** i **GND** na złączu rozszerzeń płytki
   (sprawdź opis pinów w dokumentacji Olimex).
   - Do gniazda RJ45 płytki **nigdy nie podajesz 24 V**.
   - Podczas wgrywania firmware przez USB odłącz zasilanie zewnętrzne 5 V, chyba że dokumentacja
     płytki wyraźnie dopuszcza oba naraz.

**Sprawdzenie:** po włączeniu 24 V ESP32 startuje (log, ping), a na wyjściach przetwornic jest 5 V
także z podłączonym ESP32.

---

## Krok 4. ESP32 ↔ moduł ELRS

1. **Radiomaster Nomad** zasilasz przez złącze **XT30** napięciem 6–16,8 V: dodatkowa
   przetwornica C ustawiona na **12 V** (≥2 A, ustaw napięcie przed podłączeniem, jak w kroku 3).
   Kondensator low-ESR z kroku 3 przylutuj na wejściu XT30.
   Sygnał CRSF bierzesz z pinów zatoki modułu (JR / adapter micro z zestawu): pin sygnałowy (S.Port)
   i GND. Pinu zasilania zatoki nie podłączaj, bo moduł jest zasilany z XT30.
   - Sprawdź w instrukcji Nomada poziom linii CRSF. Jeśli jest 5 V, wstaw konwerter poziomów BSS138.
2. Zlutuj połączenie half-duplex ([hardware.md](hardware.md#podłączenie-esp32--moduł-elrs)):
   ```
   GPIO4  ──[ 1 kΩ ]──┬── CRSF / S.Port modułu
   GPIO36 ────────────┘
   GND ───────────────── GND modułu (pin zatoki)
   12 V (przetwornica C) ─ XT30 Nomada
   ```
   Rezystor przylutuj bezpośrednio przy ESP32, a złącze zaizoluj koszulką.
3. Przykręć **obie** anteny Nomada (dwuzakresowe 2,4 GHz / 868 MHz) **przed** włączeniem zasilania.
   Moduł nadający bez anteny może się uszkodzić.
4. Skonfiguruj moduł (WiFi modułu lub ELRS Configurator):
   - firmware w wersji **EU / LBT**, a dla 868 MHz pasmo **EU868** (nie 915 MHz);
   - moc: 2,4 GHz **maks. 100 mW**, 868 MHz **maks. 25 mW**;
   - tryb Gemini (oba układy na jednym paśmie) albo Xrossband (2,4 GHz + 868 MHz). Odbiornik w dronie
     musi obsługiwać wybrany tryb;
   - packet rate np. 250 lub 500 Hz;
   - bind phrase taka jak w odbiorniku drona.
5. Włącz zasilanie, uruchom aplikację.

**Sprawdzenie** (log masztu w `pio device monitor`):

- `elrs=…Hz` pokazuje packet rate modułu. Jeśli jest `0Hz`, moduł nie odsyła danych: sprawdź GPIO36,
  rezystor i masę.
- `crcerr` nie rośnie.
- Dron (bez śmigieł!) się łączy. W Betaflight → Receiver kanały ruszają się zgodnie z drążkami,
  a OSD aplikacji pokazuje LQ, RSSI i napięcie baterii drona.
- Zrób testy failsafe z [bench-test.md](bench-test.md), punkt 2: zabicie aplikacji, wyjęcie
  kabla sieciowego, wyjęcie radia, odłączenie zasilania masztu.

---

## Krok 5. SteadyView V3.3 na maszcie

### 5.1 Ustal pinout, zanim cokolwiek przylutujesz

Pinout listwy modułu nie jest potwierdzony w tej dokumentacji. Z dokumentacji ExpressLRS
Backpack: piny **1 / 2 / 3 = CLK / DATA / CS**. Pozostałe ustal sam:

1. Obejrzyj nadruki przy pinach (góra i spód płytki) i zrób zdjęcie listwy z obu stron.
2. **GND:** multimetrem (ciągłość) znajdź piny połączone z metalowym ekranem modułu.
3. **5 V:** włóż moduł do gogli, włącz je i zmierz napięcie na pinach względem GND.
   Pin z ~5 V to zasilanie.
4. **Wideo:** pin, który w goglach prowadzi do wejścia wideo. Najpewniej oscyloskopem: sygnał
   ok. 1 Vpp z impulsami synchronizacji co 64 µs.
5. Zapisz wynik (np. w [hardware.md](hardware.md)). Wszystkie kolejne kroki na nim polegają.

### 5.2 Okablowanie

1. Wlutuj listwę żeńską pasującą do rastra modułu na kawałek płytki uniwersalnej albo przylutuj
   przewody bezpośrednio do pinów. Gniazdo ułatwia wyjęcie modułu do gogli.
2. Połącz:

   | Pin SteadyView | Dokąd |
   |---|---|
   | 5 V | wyjście filtra LC przetwornicy B |
   | GND | punkt masy (gwiazda) |
   | Wideo | żyła środkowa przewodu ekranowanego → środek gniazda BNC |
   | GND przy wideo | ekran przewodu → obudowa gniazda BNC |
   | CLK (1) | ESP32 GPIO32 |
   | DATA (2) | ESP32 GPIO33 |
   | CS (3) | ESP32 GPIO13 |

3. Przewód wideo prowadź możliwie krótko i z dala od przetwornic.
4. Przykręć anteny 5,8 GHz (patch + dookólna) **przed** włączeniem.

### 5.3 Test na krótkim kablu

1. Podłącz gniazdo BNC przez przejściówkę BNC → RCA **bezpośrednio** do grabbera
   (na razie bez balunów i skrętki).
2. Włącz VTX drona i ustaw w `gcs.ini` `vrx_channel` na jego kanał (np. `R1`).
3. Uruchom aplikację:
   - log masztu: `[vrx] tuned to R1 (5658 MHz)`;
   - OSD: kanał bez znaku `>`;
   - na ekranie obraz z kamery drona.
4. Sprawdź **Ctrl+←/→**: obraz powinien znikać (szum) i wracać po powrocie na właściwy kanał.
5. Zmierz opóźnienie toru wideo (procedura w [bench-test.md](bench-test.md), punkt 3).

Jeśli kanał się nie zmienia, sprawdź kolejność CLK/DATA/CS i masę. Jeśli widać paski na obrazie,
to zakłócenia z przetwornicy: popraw filtr LC i odsuń przewód wideo.

---

## Krok 6. Skrętka i rozdział par

1. Do testów rozwiń cały kabel z bębna, żeby pomiary odpowiadały warunkom w terenie.
2. Zarób oba końce wtykami RJ45 w standardzie **T568B** (ten sam na obu końcach).
3. Sprawdź kabel testerem: wszystkie 8 żył po kolei.
4. Na obu końcach włóż wtyki w przejściówki RJ45 ↔ zaciski śrubowe i rozdziel pary:

   | Para (T568B, piny) | Strona laptopa | Strona masztu |
   |---|---|---|
   | pomarańczowa (1-2) + zielona (3-6) | przejściówka → patchcord → laptop | przejściówka → patchcord → ESP32 |
   | niebieska (4-5) | balun (zaciski) | balun (zaciski) |
   | brązowa (7-8) | 24 V: **brązowa = +**, biało-brązowa = − | → bezpiecznik → przetwornice |

   Najprościej użyć dwóch przejściówek na każdym końcu: jedna podaje dalej pary danych przez
   gniazdo RJ45, druga daje dostęp do zacisków par niebieskiej i brązowej.
5. **Pomiar przed zasileniem:**
   - rezystancja pętli brązowej pary (zwarte żyły na drugim końcu) to ok. 17 Ω na 100 m
     (patrz [hardware.md](hardware.md#okablowanie-skrętki));
   - brak zwarcia między brązową parą a pozostałymi.
6. Podłącz 24 V po stronie laptopa i zmierz napięcie na końcu masztowym: najpierw bez obciążenia,
   potem z działającym masztem. Pod obciążeniem musi zostać zapas powyżej minimalnego napięcia
   wejściowego przetwornic.

**Sprawdzenie:** ping 192.168.77.2 przez cały kabel, maszt startuje zasilany po skrętce.

---

## Krok 7. Baluny wideo

1. Po stronie masztu: balun BNC → gniazdo BNC SteadyView, a zaciski balunu → niebieska para.
2. Po stronie laptopa: niebieska para → zaciski drugiego balunu, BNC balunu → przejściówka
   BNC → RCA → grabber.
3. **Polaryzacja:** na obu balunach ta sama żyła (np. niebieska) na zacisku „+”, a
   biało-niebieska na „−”. Zamiana daje odwrócony lub słaby obraz.

**Sprawdzenie:** obraz jak w kroku 5.3, bez nowych pasków ani zakłóceń. Jeśli pojawiły się paski,
sprawdź, czy przewód wideo nie biegnie równolegle tuż przy zasilaniu na maszcie (pętla masy)
i czy baluny są pasywne (izolują galwanicznie).

---

## Krok 8. Obudowa na maszcie

Obudowa do druku 3D z gotowymi otworami: [hardware/enclosure](../hardware/enclosure/README.md)
(tam też kolejność montażu wtopek, dławika i uszczelki). Poniższe punkty dotyczą też gotowej puszki IP65.

1. Rozplanuj: ESP32 i przetwornice w jednej części, SteadyView jak najdalej od przetwornic,
   moduł ELRS przy ściance (ciepło).
2. Wywierć otwory:
   - dławiki kablowe na skrętkę;
   - przepusty / gniazda SMA na anteny.
   Otwory rób **od spodu lub z boku**, nigdy od góry (woda).
3. Przymocuj płytki na dystansach lub taśmie dwustronnej. Nic nie może latać luzem.
4. Anteny:
   - wyprowadź je krótkimi przedłużkami SMA low-loss (sprawdź SMA vs RP-SMA);
   - **2,4 GHz (ELRS) i 5,8 GHz (wideo) rozsuń jak najdalej od siebie**;
   - patch skieruj w stronę obszaru lotu, dookólną ustaw pionowo.
5. Moduł ELRS się grzeje: daj mu radiator lub kontakt z metalową ścianką, a na stole sprawdź
   temperaturę po 30 minutach pracy w zamkniętej puszce.
6. Zrób **pętlę odciążającą** skrętki przed dławikiem, żeby ciężar kabla nie ciągnął za złącza.
7. Dokręć dławiki i sprawdź uszczelki.

---

## Krok 9. Maszt

1. Na ziemi przymocuj obudowę (bracket z rowkiem V) do szczytowego segmentu masztu paskami na rzep
   lub opaskami.
2. Poprowadź skrętkę wzdłuż masztu, mocując ją opaskami co ok. 50 cm. Zostaw luz przy każdym
   łączeniu segmentów, żeby rozsuwanie nie szarpało kabla.
3. Rozstaw podstawę / wbij szpilkę i przypnij **odciągi** (3 sztuki co 120°) na ok. 2/3 wysokości.
4. Rozsuwaj maszt od najwyższego segmentu, ustawiając patch w kierunku lotu przed zablokowaniem
   segmentów.
5. Sprawdź stabilność przy wietrze. Maszt nie może się kołysać więcej niż kilka centymetrów.

---

## Krok 10. Pierwsze uruchomienie całości

**Kolejność włączania:**

1. Laptop + aplikacja (OSD: `NO MAST LINK`).
2. Zasilanie 24 V masztu → OSD: `MAST WAITING`, potem `MAST ACTIVE`, gdy radio jest podłączone.
3. Radio przez USB, **przełącznik ARM w pozycji rozbrojonej** (inaczej ostrzeżenie `ARM GUARD`).
4. Dron (bez śmigieł) → OSD: LQ, RSSI, bateria, obraz.

**Checklista przed pierwszym lotem:**

- [ ] Wszystkie testy z [bench-test.md](bench-test.md) przeszły na pełnym zestawie (z masztem i skrętką).
- [ ] Failsafe: wyjęcie skrętki z laptopa → dron w failsafe (czas zależy od ustawień failsafe w Betaflight, zwykle 1–2 s).
- [ ] Opóźnienie wideo zmierzone i akceptowalne.
- [ ] Zasięg sprawdzony stopniowo (obserwator przy dronie, obserwacja LQ w OSD).
- [ ] Obserwator w zasięgu wzroku drona (wymóg przy locie FPV).

**Kolejność wyłączania:** dron → zasilanie masztu → aplikacja → radio.

---

## Rozstawianie w terenie (skrót, po zbudowaniu)

1. Rozwiń skrętkę, rozstaw maszt z odciągami (krok 9).
2. Podłącz przejściówki po stronie laptopa: patchcord → laptop, balun → grabber, 24 V.
3. Włącz w kolejności z kroku 10.
4. Sprawdź OSD: `MAST ACTIVE`, RTT, obraz, kanał VRX, LQ.
5. Zwijanie w odwrotnej kolejności. Skrętkę zwijaj na bęben bez skręcania i zagnieceń.
