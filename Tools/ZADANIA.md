# Zadania gotowe do oddania (Gemini, Codex, ktokolwiek)

Każde zadanie da się wkleić w całości jako polecenie. Zasady pracy: `AGENTS.md`.

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
