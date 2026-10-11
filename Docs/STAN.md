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

## Historia

| data | co | całość |
|---|---|---|
| 11.10.2026 | pierwsze podsumowanie: 0.5 odtworzone, Vosk w prompterze, testy na GitHubie, wygląd 3.1 | ~60% |
