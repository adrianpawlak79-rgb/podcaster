# Raport UX: ścieżka początkującego (27.11.2026)

Zrzuty: `Docs/zespol/raporty/zrzuty/ux-<tryb>[-small].png` (wszystkie tryby `--preview-ui`, 1280×800 i 960×640, Xvfb :102).
Uwaga techniczna: tryby inne niż przegląd trzeba uruchamiać **bez** pliku WAV. Podanie nagrania otwiera zawsze „Odsłuch i montaż” (`Source/Main.cpp:61-62`), więc `prompter`, `settings` czy `prepare-music` z WAV pokazują ten sam ekran.

## 1. Ocena: 6/10
Główna ścieżka przechodzi i jest po polsku, a wiele ekranów ma dobre podpowiedzi. Program jednak wciąż pokazuje sprzeczne stany (REC/NAGRYWANIE w czasie pauzy), ten sam przycisk ma różne nazwy, w małym oknie przyciski tracą podpisy, a na ekranie Gotowe nie widać najważniejszej informacji: czy plik nadaje się do publikacji. To ten sam wzorzec co w zgłoszeniach Adriana: „nie znajduję” i „nie rozumiem, co program zrobił”.

## 2. Blokery wydania

**B1. Nagranie: sprzeczne stany na jednym ekranie.**
- Gdzie: `ux-prompter-rec.png` i `ux-prompter-znaczki.png`.
- Co widać: przed startem na fali jest „● NAGRYWANIE · ostatnie 10 s”, a w zegarze czerwone „● REC”, choć nic się nie nagrywa (lampka pokazuje „SŁUCHAM”, a przycisk „Nagraj”). W `znaczki` nad wersem jest „PAUZA”, a obok są REC i NAGRYWANIE.
- Czego się spodziewa: jednego, jasnego stanu. Czerwień ma znaczyć „idzie nagranie” (`UiRules.h:84`).
- Propozycja: przed startem pokazać szare „GOTOWY · naciśnij Nagraj albo spację” (tak już wygląda `ux-replace-recording.png`). Czerwień tylko w czasie nagrywania, a w pauzie wszędzie pomarańczowe „PAUZA”.

**B2. Mały ekran (960×640): przyciski nagrania bez podpisów, lampka znika.**
- Gdzie: `ux-prompter-wskazanie-small.png`.
- Co widać: „Odtwórz”, „Powtórz wers” i „Zakończ” to same ikony ▶ ↻ ■. Lampka skurczyła się do kółka z literą „S”. Ikona ↻ nie ma podpisu nawet w dużym oknie (`ux-prompter-rec.png`).
- Dlaczego to bloker: laptop Adriana może mieć małe okno, a „nie znalazł przycisku” to powtarzające się zgłoszenie (#140).
- Propozycja: podpisy zostają zawsze, krótsze w razie potrzeby („Odsłuch”, „Powtórz (R)”, „Koniec”). Lampka ma najmniej 64 px z napisem. Przycisk ↻ podpisać „Powtórz wers (R)”.

**B3. Gotowe: głośność nie zgadza się z celem, a nikt tego nie tłumaczy.**
- Gdzie: `ux-done.png`.
- Co widać: „Jak w podcastach (mono)”, a pod spodem „−19.0 LUFS” przy celu −16 z briefu. Do tego „WAV 24-bit (MP3 nie powstał)” bez powodu na ekranie. Powód jest tylko w dymku (`ReviewComponent.h:1914`).
- Czego się spodziewa: „plik gotowy do wrzucenia”, a jeśli nie, to co zrobić.
- Propozycja: dla −19 przy mono to może być poprawne, ale trzeba to napisać wprost: „Głośność: gotowa do publikacji (mono, −19 LUFS — standard dla mono)”. Przy braku MP3 pokazać na widoku powód i przycisk „Spróbuj zapisać MP3 ponownie”.

**B4. Ta sama czynność ma pięć nazw.**
- Nazwy: „Nagraj nowe podejście” (brief, Trace.h), „Popraw fragment” (Gotowe i montaż), „Nagraj poprawkę” (`ux-replace.png`), „Nagraj od linii” (prompter) i „Wróć do nagrywania” (#141).
- Do tego nazwa ekranu ustawień: tytuł to „Ustawienia”, a dymek odsyła do „Mikrofon i sprzęt” (`ReviewComponent.h:1128`). Takiej nazwy nie ma nigdzie na ekranie.
- Propozycja: zostawić „Popraw fragment”, który otwiera wybór, oraz „Nagraj poprawkę”, który nagrywa. Dymek zmienić na „w Ustawieniach”.

## 3. Ważne, ale nie blokuje

**W1. Start: obcięte i nachodzące napisy.**
- Gdzie: `ux-witaj.png` i `ux-witaj-small.png`.
- Co widać: „Promp…” zamiast „Prompter”. Przycisk „Sprawdź sprzęt” zasłania tekst „sprzęt niesprawdzony”.
- Propozycja: etykieta w osobnym wierszu „Tekst do czytania:”, a przycisk na prawo od tekstu stanu.

**W2. Żargon dla laika.**
- Pojęcia: „Filtr dołu”, „Ekspander”, „De-esser”, „Rezonanse”, „Kompresja 2:1”, „LUFS”, „dBTP”, „szwy” (`ReviewComponent.h:143`), „Po R”, „rozbieg”, „wzorzec brzmienia”.
- Propozycja: w kaflach najpierw prosty tytuł, a nazwa fachowa mniejsza pod spodem („Bez dudnienia (filtr dołu)”, „Ostre ‚s’ (de-esser)”). „Szwy” zamienić na „miejsca sklejenia”. „Po R: 3 miejsca” zamienić na „Powtórzone wersy: 3 miejsca do sprawdzenia”.

**W3. Przełącznik „słychać: PO miksie” wygląda jak napis, a nie przycisk.**
- Gdzie: `ux-tor.png`.
- Propozycja: dwa przyciski obok siebie „Przed miksem | Po miksie”, z podświetleniem aktywnego (skrót Tab zostaje).

**W4. Miks: zaawansowane od razu na wierzchu.**
- Gdzie: `ux-tor.png`.
- Co widać: 9 kafli, suwaki i trzy brzmienia naraz. W 960×640 kafle są ucięte, a „Przywróć ustawienia domyś…” jest obcięte (`ux-tor-small.png`).
- Propozycja: domyślnie zwinięte z przyciskiem „Pokaż ustawienia zaawansowane”. Przycisk „Przywróć ustawienia domyślne” zawsze widoczny pod brzmieniami, bo tego szukał Adrian w #140.

**W5. Tryb poprawki: mylące wyjścia.**
- Gdzie: `ux-replace.png`.
- Co widać: będąc w montażu, użytkownik ma „← Wróć do montażu” oraz aktywne „Dalej: miks”, choć jest w trakcie wyboru fragmentu.
- Propozycja: przycisk nazwać „Anuluj poprawkę”, a „Dalej: miks” wyłączyć do czasu zakończenia.
- W `ux-replace-recording.png` naraz są „← Przygotowanie”, „Wstecz”, „Dalej”, „Wróć”, „A−”, „A+” i „Lustro”. „Wstecz/Dalej” przewijają tekst, a wyglądają jak nawigacja kroków. Propozycja: „↑ Tekst wyżej” / „↓ Tekst niżej”. Przycisk „← Przygotowanie” ukryć podczas poprawki.

**W6. Pasek kroków wygląda na klikalny, a nie jest.**
- Klikać można tylko kroki 0–1 i tylko poza nagraniem i przeglądem (`MainComponent.h:431`), a wszystkie kafle wyglądają tak samo.
- Propozycja: kafle nieaktywne przyciemnić i bez efektu po najechaniu. Ukończone kroki oznaczyć ✓.

**W7. Nagranie bez promptera: dwie różne nazwy jednego wyjścia.**
- Gdzie: `ux-recording.png`.
- Co widać: przycisk „Zatrzymaj i oceń”, a w prompterze to samo to „Zakończ”. „Nagrywanie trwa. Patrz na lampkę” pojawia się przy „Czekam na sygnał… Poznaję Twój głos: 0%”, czyli niejasne, czy już nagrywa.
- Propozycja: jedna nazwa „Zakończ nagranie”. Dopóki nie ma sygnału: „Czekam na mikrofon — jeszcze nie nagrywam”.

**W8. Ustawienia: niezrozumiałe elementy.**
- Gdzie: `ux-settings.png`.
- Co widać: przycisk „Test” bez opisu, czego dotyczy. Przy mikrofonie jest pusty szary pasek bez podpisu (to miernik). Słuchawki są nad mikrofonem.
- Propozycja: „Zagraj dźwięk testowy”. Pod paskiem podpis „poziom mikrofonu — powiedz coś”. Mikrofon na pierwszym miejscu.

**W9. Kreator: wybór urządzenia ukryty na dole.**
- Gdzie: `ux-wizard.png`.
- Co widać: tekst „Powiedz cokolwiek” stoi nad listami „Brak urządzenia / Brak wejść”. Bez urządzenia „Dalej” jest wyłączone i nie ma podpowiedzi, co zrobić.
- Propozycja: listy przenieść na górę z napisem „1. Wybierz mikrofon”. Przy braku urządzenia: „Nie widzę mikrofonu. Podłącz go (np. SSL 2) i kliknij Odśwież”, z przyciskiem „Odśwież”.

## 4. Drobne
- Przygotowanie (`ux-start.png`): „Dodaj tekst do promptera lub wklej go samodzielnie” znaczy to samo dwa razy. Lepiej: „Wklej tekst do czytania albo nagrywaj bez tekstu”.
- Pasek boczny przygotowania: „Test jeszcze nie wykonany”. Lepiej: „Sprzęt nie sprawdzony — zajmie 1 minutę”.
- Montaż (`ux-review.png`): komunikat „Nagranie wczytane z pliku — przesłuchałem je od nowa” jest niejasny dla laika i nie ma znaczenia przy świeżym nagraniu.
- Montaż: brak widocznego „Cofnij cięcie”, dopóki nic nie wycięto. Warto pokazywać go wyłączony, żeby wiadomo było, że cofanie istnieje.
- `ux-prompter-podklad-pauza.png`: w pauzie z podkładem lampka maleje do miniatury. Niespójne z widokiem bez podkładu.
- Gotowe: „Nazwa t2__miks-3.wav” z podwójnym podkreślnikiem wygląda na błąd. Lepiej „t2 – miks 3.wav”.
- Ikona kroku „Odsłuch i montaż” renderuje się jak „◖0” (słuchawka i zero?). Jest nieczytelna we wszystkich zrzutach.

## 5. Czego nie dało się sprawdzić
- Zachowania na żywo bez mikrofonu: odliczanie i rozbieg przy „Nagraj od linii”, faktyczne działanie R, próbka 15 s miksu, czas miksu, dialog „Odzyskane nagranie”. To wymaga karty dźwiękowej. Oceniałem z kodu i zrzutów.
- Tryb `review` z miksem w toku (pasek postępu, próbka) nie ma osobnego trybu podglądu.
- Brak trybu podglądu dla okna „Dodaj tekst”, komunikatów (AlertWindow) i ekranu startowego bez historii nagrań.
