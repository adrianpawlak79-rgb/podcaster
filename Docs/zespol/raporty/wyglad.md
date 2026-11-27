# Raport: wygląd (UI) — 27.11.2026

Zrzuty: `Docs/zespol/raporty/zrzuty/wyglad-*.png` (tryby `--preview-ui`, Xvfb :103, 1280×800 i 960×640).
Uwaga do narzędzia: podanie `Tests/t2.wav` przy trybach `start`, `prepare-music`, `prompter-*`, `replace-recording`
daje zrzut ekranu montażu zamiast właściwego trybu (fixture ładuje nagranie w konstruktorze, `Source/Main.cpp:47`).
Te tryby zrobiłem bez pliku. Tryb `done` zapisuje przy każdym uruchomieniu plik `Tests/t2__miks-N.wav` (ignorowany przez git).

## 1. Ocena: 6/10
Paleta, typografia i kafelki kroków są spójne i czytelne w 1280×800, ale w 960×640 kilka ekranów ma
nachodzące lub ucięte elementy, w tym przycisk, którego Adrian szukał w #140.

## 2. Blokery wydania
1. **Miks w 960×640: kafle zaawansowane przykryte przyciskami, „Przywróć ustawienia domyś…” ucięty.**
   Zrzut: `wyglad-tor-small.png` (vs `wyglad-tor.png`). Dolny rząd przycisków leży na kaflach (Filtr dołu, Korektor),
   a tekst panelu bocznego wychodzi pod „Wróć do montażu”.
   Kod: `Source/ReviewComponent.h:1019–1022` (pasek narzędzi), układ kafli w tym samym `resized()`.
   Propozycja: w małym oknie kafle w przewijanym `Viewport` nad paskiem przycisków; przycisk podpisany „Ustawienia domyślne”
   gdy szerokość < 1100 (linia 1042 ma już taki warunek, ale oba warianty tekstu są identyczne).
2. **Ekran startowy: przycisk „Sprawdź sprzęt” zasłania napis o stanie mikrofonu** — także w 1280
   („sprzęt niesp|”) i jeszcze mocniej w 960. Zrzuty: `wyglad-witaj.png`, `wyglad-witaj-small.png`.
   Kod: `Source/StartComponent.h:194–196` — przycisk liczony od prawej krawędzi `L.equipment`, bez znajomości szerokości tekstu.
   Propozycja: przycisk na prawym brzegu karty albo tekst skrócić/umieścić w wierszu nad przyciskiem.
3. **Ekran startowy: etykieta „Promp…” ucięta w obu rozmiarach.** Zrzuty jw.
   Kod: `Source/StartComponent.h:281` (`textRow.removeFromLeft (52)`), rysowanie `:135`.
   Propozycja: szerokość z `font.getStringWidth`, ok. 80 px, albo etykieta nad wyborem jak „Rodzaj nagrania”.
4. **Gotowe w 960×640: wartość „Gotowa głośność” znika pod przyciskami.** Zrzut: `wyglad-done-small.png`
   (w 1280 widać „Jak w podcastach (mono) / −19.0 LUFS”, `wyglad-done.png`). Kod: `Source/ReviewComponent.h:1175` (wiersze panelu)
   i układ przycisków panelu. Propozycja: mniejsze odstępy wierszy (42 → 30) w małym oknie albo przyciski w 2 kolumnach.
   Dodatkowo: na ekranie brak „Nagraj nowe podejście” (brief go wymienia; `grep` w `Source/` nic nie znajduje) — do potwierdzenia z kierownikiem.

## 3. Ważne, ale nie blokuje
5. **Prompter w 960×640: duży wskaźnik „SŁUCHAM” zapada się do kropki / litery w kółku.**
   Zrzuty: `wyglad-prompter-podklad-pauza-small.png` (pusta kropka), `wyglad-replace-recording-small.png` („G”), por. `wyglad-prompter-rec.png`.
   Kod: panel boczny w `Source/PrompterComponent.h` (rysowanie stanu). Propozycja: minimalna średnica ok. 72 px albo ukrycie i stan tylko w polu czasu.
6. **„Nagraj popraw…” ucięte (poprawka, #141/#142, 960).** `wyglad-replace-recording-small.png`.
   Kod: `Source/PrompterComponent.h:164/605`. W tym trybie obok są „Wstecz” i „Dalej” (to przewijanie tekstu, `:103`) —
   wyglądają jak nawigacja kreatora i zabierają miejsce. Propozycja: w trybie poprawki ukryć Wstecz/Dalej lub dać ikony ↑↓; podpis „Nagraj”.
   Na tym samym zrzucie napis „Tu zobaczysz falę nagrania” przy widocznej fali (`:1068`) i inny styl fali (pełne niebieskie plamy)
   niż na montażu (słupki) — niespójne.
7. **Plakietka „ZACZNIJ CZYTAĆ” na ekranie pauzy z podkładem**, a pole czasu mówi „PAUZA” (`wyglad-prompter-podklad-pauza-small.png`).
   Kod: `Source/PrompterComponent.h:1907–1915` — warunek pauzy sprawdzany po `hasStarted()`. Propozycja: najpierw stan pauzy.
8. **Ustawienia:** nagłówek „01 / PRZYGOTOWANIE” i podświetlony kafel Przygotowanie, choć to nie krok; pusty szary prostokąt obok
   „Mikrofon” (miernik bez urządzenia). `wyglad-settings-small.png`. Propozycja: eyebrow „USTAWIENIA”, kafel bez podświetlenia, miernik ukryty bez urządzenia.
9. **Ikona kroku „Odsłuch i montaż” wygląda jak „(0”** (słuchawka nieczytelna w 18 px) na wszystkich zrzutach. Kod: `Source/StepBar.h:86–88` (`drawTileIcon`).
10. **Przygotowanie (#142) w 960:** „Nie dodano tekstu — możesz czytać swobodnie.” łamie się w 2 linie mniejszym pismem
    (`drawFittedText` zmniejsza font), w odróżnieniu od podglądu Z13 (`Docs/Z13-podglady/przygotowanie-podklad-960.png`, „Tekst gotowy”).
    Zrzut: `wyglad-prepare-music-small.png`. Kod: `Source/MainComponent.h:1221`. Propozycja: krótszy tekst („Brak tekstu — czytasz swobodnie”)
    albo przyciski pod opisem.
11. **Montaż/poprawka: „Wybrano: 0:05–0:07…” wcięte o 1 spację względem linii niżej** (`wyglad-replace-small.png`, `wyglad-replace-after.png`).
    Po poprawce panel boczny ma tylko 3 linie tekstu i dużą pustą przestrzeń; brak widocznych przycisków decyzji, choć stopka mówi
    „zdecyduj o podmianie” — do sprawdzenia przez testera ścieżki.

## 4. Drobne
- Miks: przycisk „słychać: PO miksie” ma podwójną ramkę (fokus/obrys) i małą literę na początku (`wyglad-tor.png`).
- Kreator w 960: „Dalej” wyższy niż „Jeszcze raz” / „Pominij” (`wyglad-wizard-small.png`); tekst „Tutaj pojawią się…” wcięty względem tytułu.
- Gotowe: nazwa pliku `t2__miks-4.wav` z podwójnym podkreśleniem; „MP3 nie powstał” (na Linuksie oczekiwane, na Windows sprawdzić).
- Gotowe pokazuje −19.0 LUFS przy celu −16 LUFS (wartość z nagrania testowego — do oceny przez osobę od dźwięku).
- Nagranie: w nagłówku brak nazwy odcinka pod „Zielone światło” (inne ekrany ją mają, `wyglad-prompter-rec.png`).
- Odstępstwo od specyfikacji `Docs/projekt/ciemny-sprzet/04-przygotowanie.png`: pasek kroków jako kafle zamiast kółek z liniami,
  brak wyboru urządzenia w panelu bocznym przygotowania, brak dużego pola tekstu. To świadoma zmiana (#141/#142), ale spec nie zaktualizowano.

## Co jest dobrze
Paleta i kontrast: `Tests/wyglad-test` (uruchomiony z katalogu `Tests/`) — „wszystko przeszlo”. Przyciski główne (niebieski gradient),
drugorzędne i wyłączone (`Całość`, `Ustawienia` w nagraniu) są spójne na wszystkich ekranach. Kafel aktywnego kroku czytelny.
Ekran montażu i Gotowe w 1280 zgodne z podglądami Z13.

## 5. Czego nie sprawdziłem
- Windows (skalowanie 125/150 %, krój Segoe UI) — tylko Linux/Xvfb; tam ucięcia mogą być inne.
- Stany na żywo (najechanie, wciśnięcie, animacje, odliczanie) — `--preview-ui` daje statyczny obraz.
- `wyglad-test` uruchomiony z katalogu repo zgłasza „ZLE zrodlo do sprawdzenia pisma” (szuka plików względem cwd) — test zależy od katalogu.
- Test kontrastu nie wykrywa nachodzących elementów ani ucięć; problemy 1–4 przechodzą przez CI.
