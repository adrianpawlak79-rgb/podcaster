# Zielone światło (robocza nazwa repo: podcaster)

Program dla jednej osoby przy mikrofonie: lampka (zielona / żółta / czerwona) mówiąca, czy nagranie się nadaje, prompter, który jedzie za głosem, oraz ekran po nagraniu, który sam dobiera miks i wskazuje fragment do powtórzenia. Windows, C++17 + JUCE 7; całe DSP napisane od zera w `Source/DSP/`.

Stan tego repozytorium: **wersja 0.4.0**. Nowsze zmiany (ekran nagrywania z przyciskami, kreator, prompter oparty na Vosk) są w trakcie odtwarzania — kod z 0.5.x przepadł przy resecie środowiska budowania.

Zobacz `0-PRZECZYTAJ-NAJPIERW.md` (założenia i zmierzone rzeczy) oraz `README-NOWA-WERSJA.md`.

## Budowanie

Potrzebny JUCE 7 (np. 7.0.12) i CMake ≥ 3.22:

    cmake -B build -DZS_JUCE_PATH=/sciezka/do/JUCE-7.0.12
    cmake --build build

Cross-kompilacja Linux→Windows: `mingw-toolchain.cmake`. Testy bez karty dźwiękowej: `Tests/wszystko.sh`.
