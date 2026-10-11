# Zadania gotowe do oddania (Gemini, Codex, ktokolwiek)

Każde zadanie da się wkleić w całości jako polecenie. Zasady pracy: `AGENTS.md`.

Dwa sposoby:
- **automat:** treść zadania jako zgłoszenie (issue) na GitHubie z etykietą `gemini` — Gemini zrobi je w GitHub Actions i otworzy PR (`.github/workflows/gemini.yml`). W automacie pomiń linijki o tworzeniu gałęzi, commitach i wypychaniu — to robi automat;
- **ręcznie:** wklej do Gemini CLI na swoim komputerze (wtedy wszystko jak niżej).

---

## 1. Lampka w prompterze: zamienione napisy

```
Przeczytaj AGENTS.md. Utwórz gałąź gemini/lampka-napisy od main.

Błąd: w Source/PrompterComponent.h, w drawHud(), lampka pokazuje dla
stanu 1 napis „POPRAW”, a dla stanu 2 „POWTÓRZ”. Źródło prawdy to
Source/LampLogic.h: state 0 spokój, 1 powtórz, 2 popraw, 3 stop. Ekran
główny (Lamp w Source/MainComponent.h) robi to dobrze.

Popraw tak, żeby stan 1 = „POWTÓRZ”, stan 2 = „POPRAW”, i żeby kolory
znaczyły to samo co na ekranie głównym. Zmień tylko to miejsce.

Logika wyświetlania siedzi w komponencie JUCE, więc testu w Tests/ do
tego nie ma — w opisie PR napisz wprost, że zmiana nie ma testu
automatycznego, i jak sprawdziłeś ją ręcznie.

Commit po polsku: co i dlaczego. Wypchnij gałąź i otwórz PR do main.
```

---

## 2. Instalator Windows (NSIS)

Uwaga: zależy od PR „Prompter: Vosk…” (gałąź `claude/prompter-vosk`). Zacznij po jego scaleniu albo od tej gałęzi.

```
Przeczytaj AGENTS.md. Utwórz gałąź gemini/instalator.

Cel: jeden plik Zielone-swiatlo-instalator.exe zbudowany NSIS-em
(makensis działa też na Linuksie), który instaluje do
%ProgramFiles%\Zielone swiatlo:
  - Zielone swiatlo.exe (z build-win, cel ZieloneSwiatlo),
  - libvosk.dll i trzy DLL MinGW, od których zależy
    (libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll),
  - folder vosk-model-small-pl-0.22\ (cały, bez zmian).
Dodaje skrót w menu Start i opcjonalnie na pulpicie, oraz deinstalator
widoczny w „Aplikacje i funkcje”. Interfejs instalatora po polsku.

Nie commituj żadnych .exe, .dll ani modelu (są w .gitignore).
Instalator ma brać je z katalogu podanego przy budowaniu, np.
  makensis -DPLIKI=C:\sciezka\do\plikow Tools/instalator/zielone-swiatlo.nsi

Program nie wymaga tych plików do uruchomienia — bez nich prompter
liczy sylaby (Source/VoskEngine.h). Ustawienia i nagrania leżą
w katalogu użytkownika, więc deinstalator ich NIE usuwa.

Dodaj Tools/instalator/zielone-swiatlo.nsi i krótką instrukcję
Tools/instalator/JAK-ZBUDOWAC.md. W opisie PR podaj rozmiar
zbudowanego instalatora i czy instalacja/deinstalacja była sprawdzona
na prawdziwym Windowsie (jeśli nie — napisz to wprost).
```

---

## 3. Nowy wygląd według prototypu (Codex / ChatGPT) — pięć kroków, pięć PR-ów

Wzór i decyzje: `Docs/projekt/README.md`. Rób kroki **po kolei**; następny dopiero po
scaleniu poprzedniego (wszystkie dotykają wspólnych plików).

Środowisko Codex — **Setup script** (żeby budować program i robić zrzuty `--preview-ui`):

```bash
sudo apt-get update -q
sudo apt-get install -y -q g++ cmake ninja-build espeak-ng ffmpeg python3-pip xvfb \
  libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libasound2-dev
python3 -m pip install --break-system-packages -q soundfile pyloudnorm
git clone -q --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git /opt/JUCE-7.0.12
```

Budowanie i zrzut: `cmake -G Ninja -B build -DZS_JUCE_PATH=/opt/JUCE-7.0.12 && cmake --build build --target ZieloneSwiatlo`,
potem `xvfb-run -a "build/ZieloneSwiatlo_artefacts/Release/Zielone swiatlo" --preview-ui zrzut.png recording`.

### 3.1 Fundament i ekran Nagranie

```
Przeczytaj AGENTS.md i Docs/projekt/README.md, obejrzyj Docs/projekt/ekran-2.png
i Docs/projekt/prototyp.html. Gałąź codex/wyglad-1-fundament od main.

1. Paleta, krój i miary z Docs/projekt/README.md: zmień Source/UiRules.h
   (kolory, rodzaje przycisków — dodaj trzeci rodzaj: okrągły „Nagraj”) i Source/Look.h
   (krój Segoe UI). Zaktualizuj Tests/wyglad-test.cpp tak, żeby pilnował NOWEJ palety
   (kontrast WCAG, kolor główny #39CF7B, brak kolorowania przycisków poza Look.h).
2. Wspólny szkielet ekranu: górny pasek, pasek 5 kroków ze strzałkami (Source/StepBar.h),
   treść + panel boczny, stopka — jak w prototypie.
3. Ekran Nagranie (MainComponent.h, PrompterComponent.h) według ekran-2.png, ale
   z lampką zamiast samego paska poziomu, z Esc (zatrzymaj i oceń) i R (powtórz wers)
   oraz prompterem idącym za głosem — patrz tabela „Działanie programu zostaje”.

Nie zmieniaj niczego w Source/DSP/, VoskEngine.h, Recorder.h. Zachowaj końce linii (CRLF
tam, gdzie są). Uruchom bash Tests/ci.sh — ma przejść. Dołącz do PR zrzuty
--preview-ui: recording, prompter-rec, small, i porównanie z ekran-2.png.
Opis PR po polsku: co zmienione, wynik testów, czego nie sprawdzono.
```

### 3.2 Przygotowanie — `codex/wyglad-2-przygotowanie`
Ekran główny i ustawienia sprzętu (`MainComponent.h`) oraz kreator (`WizardComponent.h`)
według `ekran-1.png`, na fundamencie z 3.1. Zrzuty: `small`, `settings`, `wizard`.

### 3.3 Odsłuch — `codex/wyglad-3-odsluch`
`ReviewComponent.h` według `ekran-3.png`: duży przebieg (wysokość z
`zs::ui::review::waveHeight`), odtwarzanie, przed/po, **zdanie oceny z TakeReview**
w panelu bocznym (nie pusty kafel). Zrzuty: `review`, `review-big` na `/tmp/mowa.wav`.

### 3.4 Montaż — `codex/wyglad-4-montaz`
Tryb edycji w `ReviewComponent.h` według `ekran-4.png`: przycięcie początku i końca
na osi (`CutEdit.h`). Bez przełączników miksu — panel boczny pokazuje, co dobrał
automat (`Decide.h`), zdaniem i liczbą.

### 3.5 Gotowe — `codex/wyglad-5-gotowe`
Ekran zapisu według `ekran-5.png`: nazwa pliku, format, długość; „Zapisz miks”,
„Wróć do montażu”, „Nagraj nowe podejście”.
