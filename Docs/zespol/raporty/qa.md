# Raport QA (tester), 27.11.2026

## 1. Ocena: 6/10
Testy automatyczne przechodzą, a prawdziwe nagranie, miks, zapis WAV, podmiana fragmentu i odzysk po awarii działają na Linuksie. #142 usunęło jednak jedyną drogę do nowego podejścia. Do tego w trybie „bez tekstu” nie da się niczego poprawić, a po podmianie zostają mylące pliki i komunikaty.

## Co sprawdzono i z jakim wynikiem
- `bash Tests/ci.sh`: wszystkie testy przechodzą, łącznie z pyloudnorm. `Tests/znane-bledy.txt` jest pusty, czyli nie ma testów ukrytych jako znane błędy.
- `Tests/juce-testy.sh /home/user/build-linux`: po przebudowaniu celów przechodzą UstawieniaTest, PunchPlikTest, CieciaPanelTest i RuntimeTest.
- Brak testu: nic nie testuje `zs::replaceRecordedRange` (PunchFile.h:244), czyli operacji na plikach przy podmianie. `podmiana-fragmentu-test` sprawdza tylko DSP (`ReplaceRange`).
- Mój test ReplaceRange (`/tmp/qa/rr.cpp`): próbki poza zakresem zostają bit w bit, długości się zgadzają, przypadek „do końca pliku” działa.
- Prawdziwa aplikacja: Xvfb :104, PulseAudio null-sink jako mikrofon, pętla espeak-ng, xdotool.
  - Bez promptera: nagranie 20 s, montaż, miks Studyjny i zapis WAV działają (`zrzuty/qa-gotowe.png`).
  - Z prompterem: nagranie 22,1 s, potem „Popraw fragment” 5,0 s nagrany od nowa jako 7,7 s. Wynik ma 24,85 s (22,13 − 5,01 + 7,73, zgadza się). Oryginał jest w `surowe/` razem z `.wersy.txt` i `.tekst.txt`, czasy wersów są przeliczone (`zrzuty/qa-po-podmianie.png`).
  - Odzysk: zabiłem program (`kill -9`) w trakcie nagrania. Po starcie plik 8,2 s jest naprawiony, a program pyta o otwarcie (`zrzuty/qa-odzysk.png`).

## 2. Blokery wydania

**B1. Nie ma już „Nagraj nowe podejście” ani „Wróć do nagrywania”. Jedyna droga do nowego nagrania to „Nowy projekt”.**
- Kroki: nagraj, przejdź do Gotowe.
- Wynik: są tylko przyciski „Wróć do miksu”, „Popraw fragment” i „Nowy projekt” (`zrzuty/qa-gotowe.png`). Esc w montażu przełącza teraz do miksu.
- Oczekiwany wynik: BRIEF opisuje w Gotowe przycisk „Nagraj nowe podejście”.
- Gdzie: #142 zmieniło `ReviewComponent.h`, przycisk `newTakeButton` (~l. 373) i `backButton` (~l. 219). `onClose` (l. 136) nie jest już nigdzie wywoływany, to martwy kod.
- Uwaga: komunikat o limicie 60 min nadal odsyła do przycisku „Wróć do nagrywania”, którego już nie ma (MainComponent.h:1879).
- Propozycja: przywrócić „Nagraj nowe podejście” w Gotowe i w montażu, obok „Popraw fragment”.

**B2. W nagraniu bez tekstu nie da się nic poprawić.**
- Kroki: Start, wybierz „Bez promptera”, nagraj, potem „Popraw fragment”, zaznacz, „Nagraj poprawkę”.
- Wynik: okno „Brak tekstu… Poprawka wymaga właściwego tekstu” (`zrzuty/qa-brak-tekstu.png`, MainComponent.h:1481-1487). Razem z B1 oznacza to, że w tym trybie jedyna rada to cięcie albo nowy projekt.
- Oczekiwany wynik: poprawka bez promptera, czyli sam rozbieg i nagranie (prompter nie jest tu potrzebny), albo przynajmniej nowe podejście.
- Waga: blokuje wydanie, bo psuje deklarowany tryb nagrania bez tekstu.

## 3. Ważne, ale nie blokuje

**W1. Spacja w trybie poprawki daje pauzę zamiast zakończenia.** Potwierdzone w aplikacji.
- Kroki: włącz „Nagraj poprawkę”, poczekaj na odliczanie, naciśnij spację.
- Wynik: PAUZA, a nagranie poprawki trwa dalej (`zrzuty/qa-spacja-pauza.png`). Kończy dopiero „Zakończ”.
- Oczekiwany wynik: spacja kończy poprawkę, jak w `MainComponent::toggleRecord` (l. 1446). Spację przejmuje prompter (pauza, PrompterComponent).
- Ryzyko: pauzę z podkładem w trybie stałej długości trzeba przemyśleć (podejrzenie: przesunięcie względem muzyki).

**W2. Pliki poprawki zostają w folderze nagrań i są na liście „Ostatnie nagrania”.** Potwierdzone.
- Po „Podmień fragment” `applyReplacement` (MainComponent.h:1576) tylko czyści `pendingReplacement`. Pliki `… poprawka_….wav`, `.wersy.txt` i `.dziennik.txt` zostają.
- Na ekranie startowym widać „Odcinek QA poprawka 0:08” jako osobne nagranie (`zrzuty/qa-lista-poprawka.png`).
- Propozycja: przenieść je do `surowe/` albo pominąć na liście (StartComponent.h:84-88).

**W3. Po podmianie stary miks wygląda na aktualny.** Potwierdzone w kodzie.
- `replaceRecordedRange` (PunchFile.h:244-350) nie rusza pliku `__miks.wav`. Lista startowa oznacza nagranie jako „gotowy plik”, jeśli `__miks.wav` istnieje (StartComponent.h:101), choć miks jest sprzed poprawki.
- Propozycja: przy podmianie przenieść `__miks.wav`/`.mp3` do `surowe/`.

**W4. Zielony napis „Oryginalne nagranie zostaje bez zmian na dysku” jest nieprawdziwy po podmianie.** Potwierdzone (`zrzuty/qa-po-podmianie.png`, ReviewComponent.h:1644).
- Główny WAV jest wtedy zastąpiony, a oryginał leży w `surowe/`. Właściciel może szukać „oryginału” w złym miejscu.

**W5. Szew podmiany to nie prawdziwy crossfade.** Podejrzenie słyszalnego „pyknięcia”, potwierdzone w kodzie.
- `ReplaceRange::render` (DSP/ReplaceRange.h:51-62) w pierwszych i ostatnich 10 ms miesza nowe nagranie ze **stałą** próbką `original[a-1]` / `original[b]`. To nie jest przenikanie z ciągłym oryginałem.
- Jeśli na granicy oryginał ma szczyt (np. 0,5), do poprawki dochodzi 10-milisekundowa rampa stałej wartości, czyli słyszalny stuk.
- Propozycja: przenikanie z oryginałem `[a, a+fade)` i `[b-fade, b)`, plus test na sinusie.

**W6. Poprawka z podkładem: możliwe przesunięcie o jeden blok.** Podejrzenie, wynika z kodu, nie zmierzone.
- `startReplacement` (MainComponent.h:1521-1523) wywołuje `recorder.start()`, który zapisuje od razu (`paused=false`, Recorder.h:75). Dopiero potem `startReplacementMusicCue()` robi `recorder.pause()` (PrompterComponent.h:193).
- Wątek audio może między tymi wywołaniami dopisać jeden blok. Wtedy poprawka (stała długość, brane jest pierwsze `b-a` próbek) jest przesunięta o ten blok względem muzyki.
- Propozycja: `start` w stanie pauzy (atomowo) i test z Recorderem.

## 4. Drobne
- D1. Ekran startowy: „Sprawdź sprzęt” zasłania nazwę mikrofonu, a etykieta „Promp…” jest ucięta przy 1400×900 (`zrzuty/qa-start.png`).
- D2. Zaznaczenie fragmentu z trybu poprawki (pomarańczowy pas) zostaje na fali w kroku Miks (`zrzuty/qa-miks-zaznaczenie.png`).
- D3. `cancelReplacement` i przerwanie przez urządzenie (MainComponent.h:1602, 1826) zostawiają na dysku niedokończony plik „… poprawka”.
- D4. W trybie poprawki widać „← Przygotowanie” i zakładki kroków. Nie sprawdziłem, co się stanie, gdy klikniesz je w trakcie nagrania poprawki (ryzyko utraty stanu `correctionMode`).
- D5. R (#142, PrompterComponent.h:560) teraz zawsze tylko pauzuje i czeka na przycisk, także gdy pozycja jest pewna. BRIEF mówi „R = powtórz wers”. Trzeba to potwierdzić z Adrianem, bo zmienia zachowanie.

## 5. Czego nie dało się sprawdzić
- MP3: koder Media Foundation jest tylko na Windows. Na Linuksie wychodzi „MP3 nie powstał”, czego się spodziewałem.
- Windows i SSL 2, start okna pod innymi (#135), zawieszenie po nowym podejściu: tutaj nie ma Windows. W Xvfb bez menedżera okien okno „Odzyskane nagranie” pojawiło się **pod** głównym oknem. To raczej artefakt Xvfb, ale warto sprawdzić na Windows.
- Podkład (MusicBed, applyBackingCuts, poprawka ze stałą długością): w czasie rundy nie nagrałem prawdziwego materiału z muzyką. Sprawdziłem tylko kod (W6).
- Wyścig miks w wątku a `adoptResult`/`acceptPreview`: w kodzie nie znalazłem błędu. `acceptingFull` jest ustawiane przed `startThread()`, a `canEdit`/`mixReady` blokują przyciski w czasie pracy wątku. Nie testowałem tego pod obciążeniem.
- Polskie znaki w prompterze: xdotool nie wpisał „ń”, więc tekst testowy to „Dzie”. To ograniczenie narzędzia, nie programu.
