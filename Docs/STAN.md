# Stan prac — Zielone światło

Ostatnia aktualizacja: **11.10.2026**. Aktualizuje Claude po każdym większym kroku.

**Procenty to ocena, nie pomiar:** udział rzeczy zrobionych i sprawdzonych.
Sprawdzenie na prawdziwym mikrofonie na Windowsie liczy się jako osobny, konieczny krok.

## Całość: ok. 60% do wersji, którą można dać komuś do używania

| obszar | stan | ocena |
|---|---|---|
| **Miks i DSP** (LUFS, dynamika, de-esser, odszumianie, automatyczny dobór) | gotowe, testowane; słabe miejsca w `Tests/znane-bledy.txt`: usuwanie trzasków (6 z 12), stary automatyczny montaż | 85% |
| **Lampka w trakcie nagrania** | logika gotowa i testowana; niesprawdzana na żywym mikrofonie od ok. 3 tygodni | 75% |
| **Prompter** (licznik sylab, Vosk, R, ponowne uzbrajanie po ciszy) | testy + nagranie „Nic dwa razy” offline: wers trafiony w ~70% chwil (≤1 słowo 68,6%, ≤2 słowa 77,7%); brak testu na żywo, ustawienia do dostrojenia | 65% |
| **Ekran oceny po nagraniu** | werdykt, A/B, oś czasu, zapis działają; czeka na nowy wygląd | 75% |
| **Montaż** | ręczne wycinanie i cofanie; brak dogrywki, podkładu, zapisu projektu | 40% |
| **Nowy wygląd z prototypu** (`Docs/projekt/`) | 3.1 Nagranie — scalone (#9); 3.2 Przygotowanie — w toku (#10); 3.3–3.5 do zrobienia | 25% |
| **Testy i automaty** | `Tests/ci.sh` przy każdym PR (GitHub Actions); Codex podłączony (start ręczny w aplikacji); Gemini — darmowy limit | 90% |
| **Wydanie** | `.exe` testowy budowany na Linuksie (MinGW); brak instalatora z Vosk i modelem, brak podpisu cyfrowego | 20% |

## Następne kroki (od największego wpływu)

1. **Test na żywo z mikrofonem** (Adrian): lampka, Esc/R, prompter z Vosk — największa niewiadoma.
2. **Nowy wygląd 3.2–3.5** (Codex robi, Claude przegląda): `Tools/ZADANIA.md`, zadanie 3.
3. **Instalator** z `libvosk.dll` i modelem (`Tools/ZADANIA.md`, zadanie 2).
4. **Podpis cyfrowy** przed sprzedażą — bez niego Windows ostrzega przy każdym uruchomieniu.

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
