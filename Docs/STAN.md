# Stan prac — Zielone światło

Ostatnia aktualizacja: **13.10.2026**. Aktualizuje Claude po każdym większym kroku.

**Procenty to ocena, nie pomiar:** udział rzeczy zrobionych i sprawdzonych.
Sprawdzenie na prawdziwym mikrofonie na Windowsie liczy się jako osobny, konieczny krok.

## 12–13.10: pierwszy test na Windowsie z SM7B

Test Adriana pokazał, co naprawdę nie działało, i większość jest już poprawiona (#46–#51):
- **pre-roll** kończył się w środku poprzedniego wersu (4 z 6) — teraz zawsze w ciszy po
  usłyszanym poprzednim wersie; na tym samym nagraniu 0 złych;
- **po R człowiek zawsze wskazuje miejsce cięcia** (pasek z falą pod prompterem, strzałki,
  Enter) — program nie zgaduje, Odsłuch tylko pokazuje szwy (#49, #51);
- **miks** brzmiał gorzej niż surowe nagranie — teraz domyślny jest łańcuch „v3” Adriana
  odtworzony naszymi modułami; Adrian: „dużo lepiej” (`Docs/badania/MIKS-NA-GLOSIE.md`);
- „Wróć do montażu”, płaski przycisk „Nagraj”.

Nic z tego jeszcze nie było ponownie sprawdzone na Windowsie. Następny test: z Klark Teknik CT1.

## Całość: ok. 63–65% do wersji 1.0 (11.10, późny wieczór)

Od poprzedniej oceny (55–60%): punch and roll kroki 3–6 scalone (#26, #27, #29, #31),
usuwanie mlasków naprawione (#33), wszystkie testy przechodzą — lista znanych błędów pusta.
Nic z tego nie było jeszcze sprawdzone przez Adriana na Windowsie.

### Poprzednia ocena: ok. 55–60% (zakres powiększony 11.10 wieczorem)

Rano było ~60% przy starym zakresie. Od tego czasu skończyliśmy cały nowy wygląd, ale decyzje
Adriana dopisały do wersji 1.0 sporo pracy: nagrywanie „punch and roll”, poprawki zaznaczonych
wersów na końcu, ekran Ustawienia, zapis MP3, film z instrukcją (`Tools/ZADANIA.md`, zad. 4–7).
Według porannego zakresu byłoby ok. 70%.

| obszar | stan | ocena |
|---|---|---|
| **Nowy wygląd z prototypu** | kroki 3.1–3.5 scalone (#9, #12, #16, #20, #23) + poprawki z uwag Adriana (#21: miksowanie na cały ekran, „Pokaż w folderze”, cięcie myszką); brak ekranu Ustawienia | 85% |
| **Miks i DSP** | jak rano + barwa z pomiaru głosu i wyniki kreatora w miksie (#15); mlaski 12 z 12 zamiast 6 z 12 (#33) — **barwy nikt nie odsłuchał na surowym nagraniu**; brak MP3; poprawki na przyszłość w zad. 9 (oddechy, nasycenie, pogłos, porównanie z miksem Adriana) | 82% |
| **Prompter** | na filmie Adriana (Windows, SM7B) śledził słowa przez cały czas, wers po wersie; zapisuje czasy wersów (#24); gubi się na powtarzanych zwrotkach | 70% |
| **Lampka w trakcie nagrania** | bez zmian; test trafności na SM7B odłożony | 75% |
| **Ekran oceny po nagraniu** | werdykt mówi ile miejsc i gdzie (#18), komunikat przy miksowaniu (#21) | 80% |
| **Nagrywanie i poprawki (punch and roll)** | kroki 1–6 scalone: czasy wersów, punkt cięcia w ciszy, R wycina pomyłkę, szew sprawdza tło, pre-roll i odliczanie, znaczki ✓/!/✗ w prompterze, lampka po R od nowa (#24–#31). **Nie sprawdzone na Windowsie.** Brak: znaczki na Odsłuchu, „popraw zaznaczone na końcu” (zad. 5) | 70% |
| **Ustawienia** | wybór karty i wejścia, zapamiętany (#14); reszta zakładek nie | 20% |
| **Testy i automaty** | `Tests/ci.sh` przy każdym PR; znane błędy naprawione (`montage-test` #27, `restore-test` #33) — wszystko przechodzi | 95% |
| **Wydanie** | `.exe` budowany na Linuksie; brak instalatora, MP3, podpisu cyfrowego, filmu | 20% |

## Następne kroki (od największego wpływu)

1. **Test punch and roll na Windowsie** (Adrian, paczka `Zielone-swiatlo-krok6.zip`):
   długość pre-rollu, odliczanie, znaczki, słyszalność szwów.
2. **Test lampki na SM7B** (Adrian nagrywa 10–20 min z celowymi błędami, Claude liczy trafność).
3. **Surowe nagranie z SM7B** (1–2 min, bez wtyczek) — sprawdzenie barwy z #15; ewentualnie
   miks Adriana ze Studio One jako wzorzec barwy.
4. **Ustawienia + MP3** (zad. 6), potem **instalator** i **film** (zad. 7).
5. **Podpis cyfrowy** przed sprzedażą.

## Ustalenia z sesji Cowork (przekazanie z 11.10.2026)

Dokument przekazania z wcześniejszej sesji (Cowork) — streszczenie tego, co nie było jeszcze w repo:

- **Wersja 0.5.1 miała kilka szczegółów, których odtworzenie (#1) nie powtórzyło:**
  R w pierwszych 1,5 sylaby wersu cofa o wers wcześniej; potwierdzenie „↺ wers od początku”
  w pasku podpowiedzi; pasek podpowiedzi rysowany przed odbiciem lustrzanym;
  wysokość fali `jlimit(140, 460, …)`. Do rozważenia przy kolejnych krokach wyglądu.
- **Prompter — pomiar Cowork** (to samo nagranie „Nic dwa razy”, 153 s mowy, tekst bez zdublowanej
  zwrotki: wiersze 1–24 i 33–36): ≤1 słowo 66%, ≤2 słowa 88%, mediana 1.
  **Nasz pomiar tym samym narzędziem co program** (`Tools/zs-sledzenie.cpp`, przycięty tekst):
  ≤1 słowo 66,6%, ≤2 słowa 74,0%, wers 68,6%. Zgodne przy ≤1, różnica przy ≤2 niewyjaśniona
  (inny sposób liczenia wzorca czasów albo opóźnienia). Na pełnym pliku: 69,2% / 78,0% / 70,7%.
- **Plik `Nic dwa razy się nie zdarza.txt`** ma zdublowaną zwrotkę „Wczoraj…” (wiersze 29–36)
  i cyrylickie „е” w „Miniesz” i „twoje” — to drugie program już naprawia sam (`ScriptNormalize.h`).
- **Metoda „klas głosek”** (0% na prawdziwym głosie) odrzucona — nie odtwarzamy.
- **Pomysły jeszcze niesprawdzone:** gramatyka dynamiczna Vosk (tylko słowa tuż przed bieżącą pozycją,
  przeładowywana w trakcie) — cel ≥85% przy ≤1 słowie, wtedy podświetlanie słów.
- **Do zrobienia wg Cowork, a brak w planie wyżej:** drugie nagranie testowe (~2 min czytania
  w realnym tempie) do niezależnej walidacji — nagrywa Adrian; kalibracja `proximityDb` na SM7B;
  eksport MP3; podkład / intro-outro.
- **Gałąź `scalenie`** (wcześniejsza praca Codexa) — nie ma jej na GitHubie; jeśli istnieje,
  to tylko lokalnie.
- Sprzęt Adriana: SM7B, SSL 2, Windows.

## Historia

| data | co | całość |
|---|---|---|
| 11.10.2026 | pierwsze podsumowanie: 0.5 odtworzone, Vosk w prompterze, testy na GitHubie, wygląd 3.1 | ~60% |
| 11.10.2026 wieczór | cały nowy wygląd (3.2–3.5), wybór wejścia, barwa z pomiaru, punch and roll kroki 1–2; zakres powiększony (zad. 4–7) | ~55–60% |
