# Zielone światło 0.4.0 — wersja testowa

## Uruchomienie

Zamknij poprzednią wersję programu, następnie uruchom `Zielone-swiatlo-0.4.0-test.exe`.
Program jest przeznaczony dla 64-bitowego Windows i nie wymaga instalatora ani dodatkowych wtyczek.
Przy pierwszym uruchomieniu może pojawić się sprawdzenie sprzętu. Ustawienia i wcześniejsze nagrania pozostają zachowane.

## Nowy interfejs

- **Przygotowanie:** wyraźny przycisk nagrywania, otwieranie tekstu lub istniejącego nagrania. Sprzęt i folder zapisu w rozwijanym panelu. Lista uwag pod osobnym przyciskiem.
- **Prompter:** duży tekst, czas, wskaźnik nagrania oraz widoczne przyciski powrotu, zmiany wielkości liter, przesuwania tekstu i odbicia lustrzanego. Tekst nie nachodzi na dolne przyciski.
- **Odsłuch:** przebieg nagrania, przełączanie przed/po miksie, krótkie podsumowanie oraz zapis. Szczegóły techniczne są domyślnie schowane.
- **Montaż:** wybierz „Edytuj nagranie”. Kliknij na osi, oznacz początek, następnie koniec i wytnij zaznaczenie. Można cofnąć ostatnie cięcie. Po zmianie program ponownie oblicza miks. Oryginał na dysku pozostaje niezmieniony. Aby zachować edycję, wybierz „Zapisz miks”.
- Jedna ciemna paleta, czytelniejsze nazwy i dopasowanie układu do okna od 960 × 640.

## Poprawki działania

Poprawiono indeksowanie pomiaru szczytu międzypróbkowego, odtwarzanie końcówek nagrań, wyrównanie czasu A/B, reakcję de-essera na ciszę i zachowanie obróbki widmowej na granicach pliku. Odszumianie wymaga teraz cichego odcinka do nauki tła; brak odpowiedniego odcinka powoduje pominięcie procesu.

Zapis działa przez bufor i osobny wątek dyskowy. Wykryte utraty danych powodują komunikat błędu, zablokowanie zapisu miksu do publikacji i zapis informacji obok pliku WAV. Zabezpieczono wymianę odtwarzacza oraz buforów współdzielonych z dźwiękiem. Czasy zdarzeń są liczone od początku ujęcia.

Uruchamiane są także kanały wyjściowe urządzenia. Odsłuch importowanego pliku jest dopasowany do częstotliwości urządzenia. Źródłowe nagrania i kolejne miksy nie są nadpisywane.

## Co sprawdzono

Zbudowano plik EXE na Windows z JUCE 7.0.12 i Clang 18 (llvm-mingw). Sprawdzono zależności: plik korzysta z bibliotek systemowych Windows, bez osobnych bibliotek DSP.

W uruchomionym zestawie 17 grup testowych wykonano **233 sprawdzenia: 228 przeszło, 5 nie przeszło**. To nie jest deklaracja, że cały historyczny zestaw testów projektu jest bezbłędny.

- 7 nowych testów regresji DSP: wszystkie przeszły.
- 9 testów ręcznego wycinania: wszystkie przeszły.
- 12 testów zapisu i współbieżności na Windows: wszystkie przeszły, w tym kompletność próbek WAV, czasy ujęcia, wykrycie błędnego wejścia i 1000 wymian odtwarzacza podczas przetwarzania.
- Niezależne porównanie LUFS z pyloudnorm na 7 przebiegach: największa różnica 0,0431 LU.
- Odszumianie na wygenerowanej mowie z dodanym szumem: redukcja szumu 10,9 dB, zmiana badanego pasma mowy 0,19 dB. To test liczbowy, nie ocena odsłuchowa.
- Obejrzano obrazy wyrenderowane przez rzeczywiste komponenty programu: ekran główny, ustawienia w mniejszym oknie, prompter i odsłuch. Nie są to makiety HTML.

Pięć niezaliczonych sprawdzeń występuje także w pierwotnym kodzie na tej samej odtworzonej próbce: jedno dotyczy skuteczności usuwania trzasków (9 z 12 zamiast co najmniej 10), a cztery starego automatycznego montażu. Porównano obie wersje, zachowując te same wejścia i wymagania testów. Automatyczny planer nie jest podłączony do nowego przycisku edycji; nowy montaż opiera się na ręcznie wskazanym fragmencie i osobnych testach.

## Ograniczenia wersji

Nie wykonano odsłuchu ani sesji na Twoim fizycznym mikrofonie i słuchawkach. Podglądy interfejsu wykonano bez uruchamiania urządzeń audio. Nie jest to jeszcze wersja zatwierdzona do sprzedaży ani gwarancja jakości brzmienia. Szczególnie usuwanie trzasków wymaga odsłuchu; automat nie naprawia każdego zdarzenia.

Montaż obejmuje wycinanie i cofnięcie ostatniego cięcia. Nie ma jeszcze podmiany fragmentu dogrywką, wielościeżkowego podkładu ani zapisywania projektu edycji do późniejszego wznowienia. Wynik można zachować jako osobny WAV.

Źródła zachowują C++17 i własny DSP. Do budowania potrzebny jest JUCE 7; plik wykonywalny ma tę zależność już włączoną.

## Budowanie źródeł

CMake 3.22 lub nowszy, C++17 i JUCE 7.0.12. Przekaż `-DZS_JUCE_PATH=<folder JUCE>` przy konfiguracji. Zbuduj cel `ZieloneSwiatlo`. Test zapisu z JUCE to cel `RuntimeTest`; wymaga jako argumentu bezwzględnej ścieżki do osobnego folderu na próbne nagrania. Dostarczony EXE zbudowano przez llvm-mingw/Clang 18 z `-DCMAKE_EXE_LINKER_FLAGS=-static`.
