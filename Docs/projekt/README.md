# Wygląd programu — wzór

Wzorem jest prototyp przygotowany w ChatGPT (`prototyp.html`, otwórz w przeglądarce)
i jego zrzuty `ekran-1.png` … `ekran-5.png`. Decyzja Adriana z 11.10.2026:
**ten wygląd zastępuje paletę z akcentem #D5A879 z wersji 0.5**.

## Decyzje

1. **Wygląd 1:1 z prototypu**, także zieleń: przyciski główne, pasek kroków i przebieg
   są zielone (`#39CF7B`). Lampka nagrania odróżnia się kształtem, rozmiarem i napisem,
   nie kolorem.
2. **Działanie programu zostaje.** Z prototypu bierzemy wygląd, nie funkcje:
   | prototyp | program (zostaje) |
   |---|---|
   | pasek poziomu zamiast lampki | **lampka** (zielono/żółto/czerwono) — główna rzecz programu |
   | „Ocena podejścia” jako pusty kafel | **zdanie oceny** z `TakeReview` („powtórz fragment 1:12–1:20”) |
   | prompter domyślnie wyłączony, suwak tempa | prompter **idzie za głosem** (Vosk, licznik sylab); bez suwaka tempa |
   | przełączniki „wyrównanie głośności”, „usuwanie szumu” | **miks dobierany automatycznie** (`Decide.h`), ze zdaniem i liczbą |
   | przycisk „Pauza” | nie ma (Recorder go nie obsługuje) |
   | przycisk „Nagraj” jako koło | **tak** — koło (wyjątek od „dwóch rodzajów przycisków”) |
   | Esc / R | zostają: Esc — zatrzymaj i oceń, R — powtórz wers |
3. **Krój:** Segoe UI Variable / Segoe UI (systemowy na Windowsie), jak w prototypie.
   Inter (`Resources/Fonts`, `Fonts.h`) można usunąć, gdy już nic go nie używa.

## Paleta i miary (z prototypu)

| rola | kolor |
|---|---|
| tło | `#141719` |
| pasek (górny, stopka) | `#1B1F21` |
| panel | `#22272A` |
| kontrolka | `#30373A` |
| tekst | `#F1F4F2` |
| dobrze / główny | `#39CF7B` |
| uwaga | `#E8BD4D` |
| błąd | `#E36A63` |

Nagłówki 27–31 px, tekst 13–17 px, prompter 26–48 px; odstępy 22–32 px; promienie 4–8 px.
Proporcje okna jak w prototypie (16:10), minimum programu 960 × 640.

## Układ każdego ekranu

- **górny pasek:** znak mikrofonu, „Zielone światło”, nazwa nagrania, po prawej ustawienia;
- **pasek 5 kroków** ze strzałkami: Przygotowanie, Nagranie, Odsłuch, Montaż, Gotowe —
  zrobione z ptaszkiem, bieżący pełną zielenią, przyszłe szare;
- **treść** po lewej (etykieta „02 / NAGRANIE”, tytuł, opis), **panel boczny** po prawej;
- **stopka:** nazwa nagrania · tryb po lewej, podpowiedź następnego kroku po prawej.

## Gdzie to jest w kodzie

| ekran prototypu | kod |
|---|---|
| 1 Przygotowanie | `MainComponent.h` (ekran główny, ustawienia sprzętu), `WizardComponent.h` (kreator) |
| 2 Nagranie | `MainComponent.h` (ekran nagrywania), `PrompterComponent.h` |
| 3 Odsłuch | `ReviewComponent.h` (przebieg, A/B, ocena) |
| 4 Montaż | `ReviewComponent.h` (tryb edycji: `CutEdit.h`) |
| 5 Gotowe | `ReviewComponent.h` (zapis miksu) |
| wspólne | `UiRules.h` (paleta, kroki, klawisze — testowane w `Tests/wyglad-test.cpp`), `Look.h`, `StepBar.h` |

Zrzuty programu robi `"Zielone swiatlo" --preview-ui plik.png <tryb>`
(tryby: `small`, `settings`, `wizard`, `recording`, `prompter`, `prompter-rec`,
`review <wav>`, `review-big <wav>`; patrz `Source/Main.cpp`).
