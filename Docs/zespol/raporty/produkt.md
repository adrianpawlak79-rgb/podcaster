# Raport: Product Owner (zakres 1.0)

## 1. Ocena gotowości: 6/10
Rdzeń drogi „nagraj → popraw → zmiksuj → zapisz” jest w kodzie i dochodzi się do niego z UI; brakuje rzeczy, które laik uzna za oczywiste przy publikacji (tagi MP3, numer wersji, pomoc) i nie ma dowodu z Windows, że całość przechodzi bez zawieszenia.

## Inwentarz funkcji (stan w kodzie)
| Funkcja | Stan | Dowód |
|---|---|---|
| Ekran startowy, ostatnie nagrania (8), „Otwórz inny plik…” | działa | StartComponent.h:55, 78–105; zrzut `zrzuty/produkt-start.png` |
| Ponowne otwarcie sesji z cięciami/wersami/tekstem | działa (pliki obok WAV: `.ciecia.tsv`, `.wersy.txt`, `.tekst.txt`, `.podklad.txt`) | MainComponent.h:1592, 1621 |
| Odzysk po awarii (naprawa nagłówka WAV + dziennik) | działa przy starcie | MainComponent.h:518, 2374–2406; WavRepair.h |
| Autozapis w trakcie nagrania | działa pośrednio (zapis strumieniowy na dysk, DiskWriter.h); brak autozapisu stanu miksu | DiskWriter.h |
| Wybór urządzeń wejścia/wyjścia, kreator sprzętu | działa | MainComponent.h:325–339, 1330–1365; WizardComponent.h |
| Prompter z Vosk, nagranie bez tekstu, pauza, R, „Nagraj od linii” | działa (wg briefu i testów ci.sh) | PrompterComponent.h |
| Montaż: cięcie, wyciszenie, przycięcie, zoom, „Cofnij cięcie” (1 krok) | działa; cofanie tylko jednopoziomowe | ReviewComponent.h:330–363 |
| Podmiana fragmentu („Popraw fragment”) | połowiczne: świeże (#141/#142), bez testu na Windows | ReviewComponent.h:1479–1545 |
| Miks: 3 brzmienia, próbka 15 s, tor modułów, cel LUFS (Podcast −16 / YouTube −14 / własny) | działa | DSP/Mix/Decide.h:174–183 |
| Podkład muzyczny (słuchawki + miks z duckingiem) | działa, świeże zmiany (#139) | Backing.h, DSP/Mix/MusicBed.h |
| Eksport WAV 24-bit + MP3 (128 kbit/s domyślnie) | działa; MP3 bez tagów ID3 i okładki | Mp3.h:51; brak `ID3/TIT2/APIC` w Source |
| Komunikat, gdy MP3 nie powstał | działa (tylko „MP3 nie powstał” + powód w podpowiedzi) | ReviewComponent.h:1170, 1914 |
| Instalator NSIS z Vosk i deinstalacją | jest | Installer/zielone-swiatlo.nsi:31–82 |
| Numer wersji | nieaktualny: 0.4.0 | CMakeLists.txt:2; README.md:5 |
| Pomoc / instrukcja w programie | brak (tylko podpowiedzi w kafelkach miksu) | grep „Pomoc” → tylko Look.h, MixChainView.h |
| Sprawdzanie aktualizacji | brak | grep „aktualiz/update” → 0 trafień |
| Wielościeżkowość / goście / zdalny rozmówca | brak (świadomie poza zakresem) | — |

## 2. Blokery wydania 1.0
1. **Brak dowodu przejścia całej ścieżki na Windows (SSL 2).** Zgłoszone zawieszenie po „Nagraj nowe podejście” nie jest wyjaśnione, a #141/#142 nie miały testów na Windows. *Rozwiązanie:* scenariusz akceptacyjny (poniżej) wykonany przez Adriana na instalatorze z gałęzi wydania; 2 przebiegi bez awarii.
2. **MP3 bez tagów (tytuł, autor, nr odcinka) i bez okładki.** Plik idzie prosto do hostingu podcastu; brak tytułu wygląda nieprofesjonalnie. *Rozwiązanie (minimum):* ID3v2.3 z TIT2 (nazwa nagrania), TPE1 (autor z ustawień), TALB, TRCK; okładka APIC opcjonalnie (MOŻE). Zapis ID3 to kilkadziesiąt linii bez bibliotek. Miejsce: Mp3.cpp.
3. **Numer wersji 0.4.0 i nieaktualny README.** Instalator, okno „O programie” i zgłoszenia błędów muszą mówić „1.0.0”. *Rozwiązanie:* podbić w CMakeLists.txt:2 i Main.cpp przy scaleniu wydania; przepisać README na krótką instrukcję po polsku.
4. **Brak pomocy dla laika.** Nie ma żadnego „Jak to działa?” ani pliku instrukcji. *Rozwiązanie:* 1 strona PDF/HTML po polsku instalowana z programem + przycisk „Pomoc” w Ustawieniach (otwiera plik). Bez systemu pomocy w aplikacji.
5. **Głośność końcowa do sprawdzenia.** Ekran „Gotowe” dla `Tests/t2.wav` pokazuje −19.0 LUFS przy celu „Jak w podcastach” (−16), zob. `zrzuty/produkt-done.png`. Jeżeli to ograniczenie przez szczyt, laik musi dostać zdanie wyjaśnienia; jeśli błąd — poprawić. *Do potwierdzenia przez osobę od DSP.*

## Kryteria „gotowe do wydania 1.0” (lista sprawdzalna)
- [ ] `bash Tests/ci.sh` i `Tests/juce-testy.sh` zielone na gałęzi wydania.
- [ ] Instalator instaluje się i odinstalowuje na czystym Windows 10 i 11 (bez praw admina, jeśli tak założono).
- [ ] Scenariusz A (prompter): nowe nagranie 10 min z tekstem → pauza → „Nagraj od linii” → R → montaż (2 cięcia, cofnięcie) → miks Studyjny → WAV+MP3 → odtworzenie MP3 w zwykłym odtwarzaczu z widocznym tytułem.
- [ ] Scenariusz B: nagranie bez tekstu, z podkładem → miks z muzyką → eksport.
- [ ] Scenariusz C: nagranie 60 min — miks próbki ≤ 30 s, całość bez zawieszenia.
- [ ] Scenariusz D: zabicie procesu w trakcie nagrania (Menedżer zadań) → po starcie propozycja odzysku, nagranie odtwarzalne.
- [ ] Scenariusz E: zamknięcie programu po montażu → ponowne otwarcie z listy ostatnich → cięcia zachowane.
- [ ] Scenariusz F: odłączenie mikrofonu w trakcie → komunikat, nic nie przepada.
- [ ] „Nagraj nowe podejście” 5× pod rząd bez zawieszenia.
- [ ] Wersja 1.0.0 widoczna w instalatorze i programie; README/instrukcja aktualne.
- [ ] Wszystkie teksty po polsku, brak angielskich komunikatów JUCE w ścieżce A–F.
- [ ] Adrian sam (bez pomocy) przechodzi scenariusz A i ocenia „tak, opublikowałbym to”.

## Podział zakresu
**MUSI w 1.0:** wszystko z inwentarza oznaczone „działa”; podmiana fragmentu po teście; tagi ID3 tekstowe; wersja 1.0.0; jedna strona pomocy; scenariusze A–F.
**MOŻE w 1.0:** okładka w MP3 (APIC); wybór bitrate MP3 w ustawieniach; wielopoziomowe „Cofnij” w montażu; normalizacja dla YouTube widoczna na ekranie Gotowe.
**Po 1.0:** sprawdzanie aktualizacji (na 1.0 wystarczy ręczna instalacja nowszej wersji); rozdziały MP3; wielu mówców/goście; nagrywanie zdalne; publikacja do hostingu/RSS; transkrypcja do opisu odcinka; podpisany cyfrowo instalator (zalecane, ale kosztuje — decyzja Adriana, SmartScreen będzie ostrzegał).

## 3. Ważne, ale nie blokuje
- Brak podpisu cyfrowego .exe → ostrzeżenie SmartScreen przy instalacji. Dopisać w instrukcji „Więcej informacji → Uruchom mimo to”.
- „Cofnij cięcie” jest jednokrokowe (ReviewComponent.h:356–360); laik po dwóch błędnych cięciach nie wróci.
- Lista ostatnich pokazuje tylko 8 plików z jednego folderu (StartComponent.h:236).
- Brak autozapisu ustawień miksu/brzmienia projektu poza „Zapamiętaj to brzmienie”.

## 4. Drobne
- Ekran Gotowe: przy braku MP3 powód jest tylko w podpowiedzi (ReviewComponent.h:1914) — powinien być widoczny.
- README.md mówi o „przepadłym kodzie 0.5.x” — mylące dla nowych osób.

## 5. Czego nie sprawdziłem
- Działania na Windows i z mikrofonem SSL 2 (kontener Linux, brak karty dźwiękowej).
- Faktycznego czasu miksu 60 min (brak długiego nagrania testowego; zadanie dla osoby od wydajności).
- Czy −19 LUFS na t2.wav wynika z ograniczenia szczytu, czy z trybu podglądu (`--preview-ui`) — brak MP3 na zrzucie może też wynikać z trybu podglądu.
