# Procedura testów na stole

**Wszystkie testy bez śmigieł.** Przejdź każdy punkt przed pierwszym lotem i po każdej zmianie firmware.

## 0. Sama aplikacja (bez sprzętu)

- [ ] `ctest --test-dir build` przechodzi.
- [ ] `python3 tools/fake_mast.py` + `mast_ip = 127.0.0.1`: OSD pokazuje `MAST ACTIVE`, RTT, LQ i baterię.
- [ ] F2: każdy drążek i przełącznik rusza właściwym kanałem.
- [ ] Start aplikacji z przełącznikiem ARM w pozycji „uzbrojony”: kanał ARM stoi nisko,
      jest ostrzeżenie `ARM GUARD`. Po przełączeniu na „rozbrojony” blokada znika.
- [ ] Wyciągnięcie radia z USB: symulator pokazuje `FAILSAFE`, OSD `NO RADIO`.

## 1. ESP32 + moduł ELRS (bez drona)

- [ ] Monitor szeregowy (`pio device monitor`) pokazuje `[eth] link up`.
- [ ] `ping 192.168.77.2` z laptopa działa.
- [ ] Po uruchomieniu aplikacji log masztu: `ACTIVE rc=500/s crsf=…/s`.
- [ ] Log pokazuje `elrs=…Hz` zgodne z Packet Rate modułu. Jeśli jest `0Hz`, moduł nie odsyła
      synchronizacji: sprawdź połączenie RX i rezystor.
- [ ] `crcerr` nie rośnie. Jeśli rośnie, sprawdź masę, długość przewodów i poziomy napięć.
- [ ] Moduł ELRS świeci/sygnalizuje, że dostaje dane z „radia” (patrz dokumentacja modułu).

## 2. Z dronem (bez śmigieł!)

- [ ] Bind odbiornika z modułem (bind phrase w ELRS). Betaflight Configurator → Receiver: kanały
      ruszają się płynnie i w dobrą stronę.
- [ ] OSD pokazuje LQ/RSSI i napięcie baterii z drona.
- [ ] **Failsafe: zabicie aplikacji** (Alt+F4 / kill): w ciągu ~0,2 s plus timeout ELRS dron
      zgłasza failsafe (Betaflight: `RXLOSS` / failsafe w zakładce Receiver).
- [ ] **Failsafe: wyciągnięcie kabla sieciowego**: jak wyżej.
- [ ] **Failsafe: wyciągnięcie radia z USB**: jak wyżej.
- [ ] **Failsafe: odłączenie zasilania masztu**: jak wyżej.
- [ ] Po każdym z powyższych i powrocie łączności: dron nie uzbraja się sam. Arm guard wymaga
      przełączenia ARM na „rozbrojony”, a Betaflight blokuje uzbrojenie po failsafe.

## 3. Wideo

- [ ] OSD pokazuje `VID 720x576 25fps` (PAL) lub `720x480 30fps` (NTSC).
- [ ] **Pomiar opóźnienia:** kamera drona patrzy na stoper w telefonie (np. strona ze stoperem
      z milisekundami), obok ekran laptopa. Drugim telefonem zrób zdjęcie obu naraz. Różnica
      wskazań to opóźnienie całego toru wideo. Powtórz kilka razy. Porównaj grabbery i `vsync = 0/1`.

## 4. W terenie

- [ ] Test zasięgu: drugi pilot/obserwator, stopniowe oddalanie, obserwacja LQ w OSD.
- [ ] Sprawdź nagrzewanie się elektroniki na maszcie (moduł 1 W w zamkniętej puszce).
