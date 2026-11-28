# Plan wydania 1.0 (kierownik, 27.11.2026)

Źródło: 7 raportów w `Docs/zespol/raporty/`. Oceny: produkt 6, UX 6, wygląd 6, QA 6, dźwięk 7, integracja 6, kreatywny 6.

Stan: główna ścieżka „nagraj → popraw → zmiksuj → zapisz” działa i jest sprawdzona prawdziwym nagraniem w kontenerze (QA). `Tests/ci.sh` i testy JUCE przechodzą, a instalator Windows się buduje. Do wydania brakuje trzech rzeczy:
- naprawy przepływu poprawek po #142;
- jakości miksu przy hałasie w tle;
- formalności wydania.

## Decyzje Adriana

Ustalone 27.11:
- D1 = B: ekran „Made with JUCE”. Najpierw sprawdzić warunki JUCE 8; jeśli przesiadka jest prosta i wymogu nie ma, przesiąść się.
- D2 = stereo −16 LUFS.
- D3: na razie bez podpisu; instrukcja „Uruchom mimo to”; każdą wydaną wersję zgłaszać do Microsoftu (fałszywe alarmy Defendera). Podpis przed sprzedażą.
- D4: docelowo własna strona do sprzedaży. GitHub Release tylko jako miejsce, z którego plik trafia na stronę.

Pierwotne pytania:
| # | Pytanie | Rekomendacja zespołu |
|---|---|---|
| D1 | Licencja JUCE: A GPLv3 (kod publiczny) / B ekran „Made with JUCE” / C licencja Indie (płatna) | A, jeśli repo może być publiczne; inaczej B na 1.0 |
| D2 | Nagrania bez podkładu: stereo −16 LUFS czy mono −19 LUFS z wyjaśnieniem | stereo −16 |
| D3 | Podpis kodu na 1.0 czy instrukcja „Uruchom mimo to” | instrukcja na 1.0, certyfikat po 1.0 |
| D4 | Gdzie publikujemy instalator (GitHub Releases?) | GitHub Release z tagu `v1.0.0` |

## Blokery 1.0 i przydział

**Fala 1 (w toku):**

| Zadanie | Gałąź | Model | Źródło |
|---|---|---|---|
| Nazwa „Zielone światło”, odesłania do nieistniejącego miejsca, ekran startowy, drobne teksty | `claude/teksty-start` | lżejszy | kreatywny, UX, wygląd |
| Okno 960×640 (miks, Gotowe, nagranie), prawdziwy stan REC/PAUZA | `claude/maly-ekran` | mocny | wygląd, UX |
| Tło (TV) nie rośnie po miksie, ogon podkładu z wyciszeniem, ostrzeżenie o przesterze | `claude/miks-tlo` | mocny | dźwięk |

**Fala 2 (po scaleniu fali 1, bo dotyka tych samych plików):**

| Zadanie | Model | Źródło |
|---|---|---|
| **Przepływ poprawek.** Wraca droga „Nagraj nowe podejście”. Poprawka fragmentu działa bez tekstu. Spacja kończy poprawkę. Pliki „poprawka” nie pojawiają się na liście nagrań. Stary miks jest unieważniany. Prawdziwe przenikanie na szwie (`ReplaceRange.h`). Test operacji na plikach | mocny | QA 1–5 |
| **Jedna nazwa dla jednej czynności.** Słowniczek: nagranie / poprawka / nowe podejście / projekt | lżejszy | kreatywny, UX |
| **Zawieszenie okna.** Wczytywanie podkładu nie blokuje okna (`MainComponent.h:1710`, `sleep` na wątku okna) | mocny | integracja |
| **Wydanie.** Wersja z CMake w programie, 1.0.0, pełne licencje (Apache, OFL), SHA-256 dla Vosk, `.gitattributes`, sprzątanie toolchaina, Release z tagu | średni | integracja |
| **Gotowe.** Jasny werdykt „gotowe do publikacji” zamiast LUFS; powód, gdy MP3 nie powstał | średni | UX, produkt |
| **Tagi ID3 w MP3** (tytuł, autor, numer odcinka) | średni | produkt |
| **D2:** stereo −16 (po decyzji) | mocny | dźwięk |

**Fala 3 (przed wydaniem):**
- Scenariusze akceptacyjne A–F z `raporty/produkt.md`, przechodzi je Adrian na Windows z SSL 2.
- Krótka pomoc w programie: jeden ekran „Jak to działa” albo podpowiedzi na każdym kroku.
- Miks dla laika: zaawansowane kafle domyślnie zwinięte, prostsze słowa.

## Po 1.0
- Podział wielkich plików (MainComponent, ReviewComponent, PrompterComponent).
- Podpis kodu.
- Sprawdzanie aktualizacji.
- Okładka w MP3.
- Szybsze otwieranie długich nagrań.

## Zasady pracy zespołu
- **Gałęzie:** każde zadanie na osobnej gałęzi `claude/<temat>`.
- **Warunki scalenia:** `Tests/ci.sh` = 0, zrzuty przed i po przy zmianach wyglądu, liczby z pomiaru przy zmianach dźwięku.
- **Scalanie:** kierownik przegląda każdą gałąź, scala ją i buduje instalator do testów dla Adriana.

## Wersja 1.1: AI (decyzja Adriana 27.11)
Tylko darmowe, otwarte rozwiązania, albo własna sieć neuronowa, jeśli da się ją sensownie napisać. Każda zmiana najpierw jako badanie z pomiarem.

1. **Rozpoznawanie mowy po nagraniu.** whisper.cpp działa lokalnie i ma licencję MIT. Badanie:
   - jakość po polsku;
   - wykrywanie powtórek i „yyy”;
   - czas liczenia na laptopie;
   - rozmiar modelu.

   Cel: podpowiedzi w montażu, a potem montaż przez tekst i transkrypcja.
2. **Odszumianie neuronowe.** Kandydaci: RNNoise (BSD), DeepFilterNet (MIT/Apache) albo własna mała sieć. Porównanie z naszym odszumianiem na nagraniu z telewizorem: liczby i pliki do odsłuchu.

Start badań po scaleniu fali 2. Badania nie mogą blokować wydania 1.0.
