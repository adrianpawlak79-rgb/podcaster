# Wygląd 3.3 — Odsłuch

Ekran korzysta ze wspólnego nagłówka i stopki oraz paska z bieżącym krokiem
„Odsłuch”. Po lewej jest przebieg z osią zdarzeń, odtwarzanie i przełącznik
przed/po. W panelu bocznym są werdykt z `TakeReview::verdictLine()`, wynik
pomiaru, miejsca do powtórki z `toRedo()` i uzasadnienia z `says()`.
Panel przewija się, więc dłuższe uzasadnienia i lista fragmentów pozostają
osiągalne także przy 960 × 640. Czerwone miejsca na osi odpowiadają powtórkom.

Kliknięcie przebiegu przenosi odsłuch. Spacja odtwarza/zatrzymuje, Tab przełącza
przed/po, N przechodzi do następnego miejsca do poprawy, Esc wraca do nagrywania.
Miks nadal dobiera się automatycznie. Szczegóły miksu, zapis i istniejąca edycja
cięć pozostają dostępne; przejście do montażu otwiera tę edycję. Komunikaty,
w tym potwierdzenie zapisu, są widoczne również przy otwartych szczegółach.
Przebieg zachowuje wysokość z `zs::ui::review::waveHeight`.

## Poprawki po 3.2

- Stopka pokazuje nazwę, bez nieistniejącego wyboru „Podcast”.
- Ustawienia urządzeń zajmują szeroką część treści. Przy 960 × 640 listy mają
  miejsce na nazwę, a panel boczny dodatkowo pokazuje nazwy wybranego mikrofonu
  i słuchawek, zawijając dłuższy tekst. Zmiana urządzenia odświeża ten panel.
  Poprawiono również tłumaczenie JUCE `none` na „brak urządzenia”.
- Kreator pokazuje w panelu wyniki zakończonych pomiarów tła, ewentualnego
  przydźwięku, szczytów mowy i przesłuchu słuchawek. Dane pochodzą z
  `SetupWizard::result()`. Wartości domyślne nie udają pomiarów: w pierwszym
  kroku widoczna jest zapowiedź wyników.

## Zrzuty

Natywna aplikacja JUCE na Linuksie pod Xvfb; bez urządzenia audio.

| tryb | rozmiar | dane |
|---|---|---|
| [review](review.png) | 1280 × 800 | `/tmp/mowa.wav` z `Tests/ci.sh` |
| [review-big](review-big.png) | 1600 × 1200 | ten sam plik |
| [settings](settings.png) | 960 × 640 | brak fizycznego urządzenia |
| [wizard](wizard.png) | 1280 × 800 | pierwszy krok, bez pomiarów |

Zrzuty wykonano przez `--preview-ui plik.png tryb`, z dodatkowym argumentem
`/tmp/mowa.wav` dla obu odsłuchów. Sprawdzono też podgląd `review-small`
przy 960 × 640.

## Weryfikacja

- Natywny build Release aplikacji: poprawny.
- `bash Tests/ci.sh`: kod 0; [wyniki](testy.md). `restore-test` i `montage-test`
  pozostają znanymi błędami. Bez nowych regresji.
- Dodatkowy test komponentów JUCE: 20 sprawdzeń, 0 błędów. Objął szerokość
  list urządzeń, otwarcie starszego nagrania, obecność oceny, Tab/Spację/Esc,
  skok i wskazanie pozycji po kliknięciu przebiegu, kontrolki cięć przy minimum,
  obsługę nieczytelnego pliku oraz zachowanie pomiarów kreatora po przejściu
  do następnych kroków. Próbka z celowo dodanym przesterem potwierdziła listę
  powtórek w panelu, aktywną nawigację i czerwone oznaczenie na osi.
- Zachowano CRLF w zmienionych plikach, które go używały.
- Nie zmieniono `Source/DSP/`, `VoskEngine.h`, `Recorder.h` ani wersji programu.

W trakcie pracy scalono do `main` #14 (wybór karty i wejścia) oraz #15
(nowy miks). Gałąź zaktualizowano do tego stanu. Zachowano `setRoom()`, pole
`wish`, oba wywołania `review.run(wish)` oraz przekazywanie wyników pokoju
z Settings. Wybór karty i wejścia, informacja o wykrytym wejściu oraz pamiętanie
ustawień urządzenia z #14 pozostają dostępne.

Nie sprawdzono aplikacji na Windowsie, fizycznego mikrofonu i słuchawek,
odsłuchu przez prawdziwy interfejs audio ani pełnego kreatora na sprzęcie.
Dodatkowe sprawdzenie kreatora użyło sygnałów syntetycznych, a próba odtwarzania
czytała wyjście callbacku do bufora w pamięci.
