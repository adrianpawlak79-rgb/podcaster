# Wygląd 3.2 — Przygotowanie

Ekran przygotowania korzysta ze wspólnego `ScreenFrame` i pierwszego kroku
na pasku. Po lewej są nazwa i edytowalny tekst; po prawej urządzenie,
poziom wejścia, sprawdzenie sprzętu, folder sesji, uwagi, otwieranie nagrania
oraz przejście do nagrywania. Przejście nie uruchamia zapisu: służy do tego
przycisk „Nagraj” na następnym ekranie. Tekst wpisany lub wczytany trafia do
promptera, pusty tekst prowadzi do nagrywania bez promptera. Można wrócić do
przygotowania przed nagraniem.

Nazwa opisuje bieżące podejście w nagłówku i stopce. Nazwy plików sesji nadal
nadaje Recorder. Wpisanego tekstu i nazwy nie zapisujemy jako ustawień sprzętu;
ostatni wybrany plik TXT nadal jest pamiętany przez Settings.

Pole „Rodzaj nagrania Podcast/Wiersz” pominięto: program nie podejmuje na jego
podstawie żadnej decyzji. Zachowano automatyczny miks i podążanie za głosem.

Kreator ma wspólny nagłówek, pasek pięciu kroków, kartę pomiaru i przyciski
w panelu bocznym. Jego sześć kroków, pomiary, ostrzeżenia i zapis wyników
pozostają te same. Narzędzia promptera A−/A+, cofanie/dalej, lustro i powrót
chowają się podczas nagrania i wracają po zatrzymaniu. Klawisze nadal działają.

## Podglądy

Zrzuty natywnej aplikacji JUCE na Linuksie, bez urządzenia audio:

- [small](small.png): przygotowanie, minimum 960 × 640.
- [settings](settings.png): ustawienia sprzętu, 960 × 640.
- [wizard](wizard.png): pierwszy krok kreatora, 1280 × 800.
- [prompter-rec](prompter-rec.png): nagranie, 1280 × 800, narzędzia schowane.

Każdy wykonano przez `--preview-ui plik.png tryb` pod Xvfb.

## Sprawdzenie

`bash Tests/ci.sh` zakończył się kodem 0. Wyniki w [testy.md](testy.md).
`restore-test` i `montage-test` pozostają na liście znanych błędów.
Natywny build Release aplikacji zakończył się powodzeniem. Dodatkowe sprawdzenie
komponentów JUCE (9 sprawdzeń, 0 błędów) potwierdziło przejście bez tekstu,
z tekstem wpisanym w edytorze, powrót do przygotowania, ukrycie sześciu narzędzi
podczas zapisu, działanie strzałki i powrót narzędzi po zatrzymaniu.
Zachowano CRLF w zmienionych nagłówkach. Nie zmieniono DSP, VoskEngine ani Recorder.

Nie sprawdzono działania na Windowsie, rzeczywistego mikrofonu, dynamicznej
lampki ani podążania promptera za prawdziwym głosem. Podgląd kreatora pokazuje
pierwszy krok bez sygnału; nie jest dowodem pomiaru ani przejścia całego kreatora.
