# Symulacja sesji bez człowieka

Pomysł Adriana (11.10.2026): sprawdzać program kompleksowo, bez nagrywania za każdym razem.

## Jak to działa

1. `Tools/zs-scenariusz.py KATALOG` — sztuczne nagranie (espeak-ng, 12 wersów, 65 s) z zapisaną
   **prawdą** w `prawda.tsv`: kiedy który wers, gdzie pomyłka i R (3×, w tym dwa razy na jednym
   wersie), gdzie wada sprzętu (przester, plozja, brak sygnału 0,6 s, mlaski).
2. `zs-symulacja KATALOG [raport.md]` (cel CMake `ZsSymulacja`, potrzebuje JUCE) — puszcza
   nagranie przez **te same klasy co program**: `Recorder` (zapis, TakeWatch, licznik sylab),
   lampka (`LampLogic`), prompter (`Follower`, `PrompterLines`, `LineLog`, R jak w
   `PrompterComponent::repeatLine`), pre-roll, znaczki, po zatrzymaniu `applyPunch`, ocena
   i miks (`TakeReview`). Porównuje z prawdą i pisze raport. Czas: ok. 8 s na przebieg.
3. Dwa przebiegi:
   - **A — prompter na samych sylabach** (tak działa program bez modelu mowy),
   - **B — prompter jak z rozpoznawaniem mowy** (pozycja z prawdy, spóźniona o 0,78 s jak Vosk)
     — żeby R, wycięcie i znaczki ocenić niezależnie od jakości śledzenia.
4. GitHub Actions (`.github/workflows/symulacja.yml`) robi to przy każdym PR; raport jest
   w podsumowaniu przebiegu. Na razie raport, nie bramka.

**Waga (19.10.2026, audyt 14.10):** każde sprawdzenie ma wagę, a na końcu raportu jest tabela
„Według wagi” i lista niezgodnych krytycznych i ważnych. Jedna liczba „61 z 71” mówiła za mało:
utrata dobrego słowa i niewycięty mlask to zupełnie inne konsekwencje.

- **krytyczne** — stracony albo zepsuty materiał: wycięte dobre czytanie, plik po cięciu nie
  powstał, coś zapisało się na pauzie, cięcie bez modelu mowy;
- **ważne** — pomyłka zostaje w nagraniu, wada niezgłoszona, złe miejsce cięcia, słyszalne
  połączenie, miks poza celem;
- **drobne** — wygoda: który wiersz pokazuje prompter, fałszywy alarm, ocena wiersza.

Pierwszy przebieg z wagami (syntezator, main po #84): krytyczne 9/9, ważne 27/29 (dwa razy mlaski
niewyczyszczone przez miks), drobne 25/33.

## Czego nie sprawdza

Wyglądu (od tego są zrzuty `--preview-ui`), prawdziwego rozpoznawania mowy (Vosk), brzmienia.
Mowa z syntezatora nie mlaska, nie oddycha i nie brzmi jak SM7B — **liczby śledzenia na
sylabach mogą być gorsze niż na żywym głosie** (sprawdzić na nagraniu Adriana).

## Pierwszy wynik (11.10.2026, main po #33)

**Przebieg B (śledzenie jak z modelu mowy): wszystko, co zależy od R, działa.**
Wers po R właściwy 3 z 3, pre-roll kończy się w pauzie przed wersem 3 z 3, z pomyłek wycięte
6,90 z 6,90 s, z dobrego czytania 0,00 s, znaczki 12 z 12 zgodne, przester / plozja / brak
sygnału zgłoszone, mlaski naprawione w miksie (3).

**Znalezione rozjazdy — do naprawy osobnymi PR-ami:**

1. **Miks wychodzi na −23 LUFS zamiast −16.** Ekran oceny zawsze miksuje w trybie „Auto”
   (`Decide::Destination::Auto`; w interfejsie nie ma wyboru celu). Auto celuje tam, gdzie nie
   trzeba ruszać najgłośniejszego szczytu — a jeden szczyt (tu: przester albo plozja) obniża
   cel dla całego nagrania do dolnej granicy −23 LUFS. Podcast powinien wyjść −16.
   Propozycja: domyślnie „Podcast” (−16) i wybór celu w Ustawieniach (zad. 6), albo szczyt
   liczony odpornie (percentyl zamiast maksimum).
2. ~~Dwa fałszywe alarmy „dryf poziomu”~~ — **nie błąd programu.** Oba to frazy, w których
   siedzi wstawiony przester i plozja: fraza jest głośniejsza, a zdarzenie frazy ma czas jej
   POCZĄTKU (1–1,5 s przed wadą). Symulator nie łączył ich z wadą; poprawione — zdarzenie liczy
   się jako wyjaśnione, gdy leży w tym samym przeczytanym kawałku co wada.
3. **Bez modelu mowy śledzenie nie działa — potwierdzone na prawdziwych głosach** (niżej).
   Na syntezatorze 22% klatek z właściwym wersem; skutki: R cofa do złego wersu, wycięcie
   zabiera dobre czytanie, znaczki trafiają w złe wersy. Wniosek: **model mowy (Vosk) jest
   warunkiem punch and roll**. Bez niego program nie powinien proponować R albo powinien
   cofać do wersu z `LineLog`, nie z licznika sylab.
4. **Jeden „słyszalny szew”** w przebiegu B przy stałym tle −70 dBFS — prawdopodobnie
   porównanie poziomów łapie końcówkę mowy przy szwie. Sprawdzić `Punch` (seams).

## Prawdziwe głosy (Wolne Lektury, 11.10.2026)

`Tools/zs-prawdziwe.py KATALOG MODEL_VOSK zs-sledzenie` pobiera krótkie wiersze z audiobookami
Wolnych Lektur (teksty w domenie publicznej, nagrania CC BY-SA 3.0; **nie trzymamy ich w
repozytorium**), wycina zapowiedź lektora i robi `prawda.tsv` z czasów słów Vosk dopasowanych
do tekstu (`zs-sledzenie --prawda`). 11 wierszy, 8 lektorów (2 kobiety), 42–121 s, 10–40 wersów;
plus „Między nami nic nie było” z tekstem w zapisie Adriana (pusty wiersz po każdym wersie).

Właściwy wers w czasie czytania:

| wiersz (lektor) | same sylaby | sylaby ±1 wiersz | Vosk ≤1 słowo | Vosk ≤2 słowa | Vosk wers |
|---|---|---|---|---|---|
| Kochanowski, fraszka (M. Michalski) | 14% | 48% | 71% | 90% | 87% |
| Krasicki, Jodła i jabłoń (M. Bogucka) | 13% | 48% | 59% | 80% | 83% |
| Konopnicka, Pszczoły (M. Rzepka) | 22% | 48% | 71% | 94% | 80% |
| Baudelaire, Olbrzymka (J. Niemirska) | 20% | 42% | 68% | 84% | 82% |
| Asnyk, Między nami… (P. Głowacki) | 9% | 73% | 70% | 88% | 78% |
| to samo, tekst Adriana | 9% | — | 71% | 89% | 78% |
| Leśmian, Magda (W. Korzeniewski) | 68% | 94% | 66% | 88% | 83% |
| Kochanowski, Pieśń IX (K. Skonieczny) | 50% | 81% | 60% | 82% | 75% |
| Mickiewicz, Góra Kikineis (P. Głowacki) | 16% | 68% | 63% | 88% | 88% |
| Mickiewicz, Widok gór… (J. Dravnel) | 0% | 51% | 79% | 96% | 90% |
| Krasicki, Wilk pokutujący (B. Mazur) | 12% | 31% | 77% | 95% | 88% |
| Jachowicz, Wróbelek… (M. Rzepka) | 4% | 39% | 69% | 87% | 70% |

**Mediana: same sylaby 13% wersu, Vosk 82% wersu (≤2 słowa: 88%).**

Uwagi:
- Wzorzec czasów robi ten sam model Vosk, więc liczby Vosk są raczej optymistyczne
  (to samo zastrzeżenie co przy pomiarze „Nic dwa razy”: tam 66% ≤1 słowo, tu 59–79%).
- „Sylaby ±1 wiersz” dla tekstu Adriana jest nieporównywalne — puste wiersze między wersami
  zmieniają numerację wierszy źródła (wers obok to dwa wiersze dalej).
- Tekst z pustym wierszem po każdym wersie śledzi się tak samo jak zwarty (Vosk 71/89/78%
  wobec 70/88/78%) — taki zapis jest bezpieczny.
- Słowa, których model nie zna (np. „powojów”, „zraszających”), Vosk pomija — dopasowanie
  83–91% słów tekstu.

## Czy R trafia we właściwy wers z rozpoznawaniem mowy (12.10.2026)

Dwa pomiary na tych samych nagraniach lektorów, przez prawdziwe hipotezy Vosk i `WordTracker`
(tak jak program, bez kompensacji opóźnienia); R cofa tak jak `PrompterComponent::repeatLine`.

**1. R w losowej chwili czytania** (`ZS_R=1 zs-sledzenie …`, co 0,1 s; lektor czyta dalej):
właściwy wers ok. 79% (65–88%), o wers za daleko wstecz ok. 18%; **w pierwszej sekundzie
wersu tylko ok. 45%** — rozpoznawanie jest 0,6–0,8 s z tyłu i „siedzi” jeszcze w poprzednim
wersie. To górna granica trudności, bo nikt nie wciska R bez przerwania.

**2. R po przerwaniu** (`Tools/zs-r-pauzy.py` + `zs-sledzenie --r`): po wybranym słowie
wstawiamy 1 s ciszy („pomylił się i przerwał”), R pada 0,5 s w tę ciszę. Pierwsze słowo
każdego wersu i słowo ze środka; 11 nagrań, 383 próby:

| gdzie pomyłka | właściwy wers | o wers wstecz | o wers naprzód |
|---|---|---|---|
| na pierwszym słowie wersu | **176/196 = 90%** | 17 | 3 |
| w środku wersu | **180/187 = 96%** | 1 | 6 |

Co znaczą błędy:
- **wstecz** — prompter wraca o wers za daleko; człowiek czyta jeszcze raz poprzedni wers
  i ten z pomyłką. Wycięcie zabiera oba stare podejścia, więc **plik i tak jest poprawny**,
  kosztem kilku sekund czytania więcej.
- **naprzód** (9 z 383, 2%) — prompter pokazuje następny wers; pomyłka zostaje w nagraniu,
  bo wersu „naprzód” jeszcze nie było. Widać to od razu na ekranie (strzałka ↑ poprawia).

Sprawdzone i odrzucone: R do ostatniego **usłyszanego** słowa zamiast pozycji promptera —
wynik identyczny (po 1,5 s ciszy prompter i tak stoi na ostatnim usłyszanym słowie).

## Pełna próba wycięcia na prawdziwym głosie (12.10.2026)

`Tools/zs-r-pauzy.py … --powtorka` + `zs-sledzenie --r-ciecie`: na 11 nagraniach lektorów
po słowie z „pomyłką” 1 s ciszy, R w tej ciszy, potem lektor czyta wers **jeszcze raz**
(wklejone jego własne nagranie od początku wersu). Dalej dokładnie to, co program po
zatrzymaniu: hipotezy Vosk → `WordTracker`, `LineLog`, `CutFinder`, `Punch::plan`, `Punch::guard`.
Cięcie „niebezpieczne” = zabiera coś spoza [koniec poprzedniego wersu, początek powtórki].

| 354 R | dobre cięcie | częściowe | bez cięcia | **niebezpieczne** |
|---|---|---|---|---|
| bez strażnika (tak jak w `main` do 12.10) | 95 (27%) | 25 | 169 | **65 (18%)** |
| ze strażnikiem | 71 (20%) | 19 | 246 | **18 (5%)** |

Przyczyny niebezpiecznych cięć:
- **szukanie ciszy przed wersem** (`CutFinder`) trafia czasem w pauzę po przecinku w
  POPRZEDNIM wersie i ucina jego końcówkę (0,4–2 s) — strażnik tego nie widzi, bo te słowa
  rozpoznawanie usłyszało przed cięciem;
- **rozpoznawanie myli podobne wersy** („Patrz, do moich…” / „Patrz, jak widząc…”) i przypisuje
  powtórkę innemu wersowi — wtedy wycinanych słów strażnik po prostu nie widzi.

Decyzja: **automatyczne wycinanie wyłączone** (`Settings::punchAutoCut`, domyślnie false).
R cofa tekst i zapisuje miejsce; po zatrzymaniu program wypisuje miejsca do wycięcia, nagranie
zostaje w całości. Włączymy, gdy cięcia będą sprawdzane dokładnymi czasami słów (rozpoznanie
całego nagrania po zatrzymaniu) i ta próba da 0 niebezpiecznych.

## Pewne / do wskazania (pomysł Adriana, 12.10.2026)

Automat tnie sam tylko to, czego jest pewien (`Punch::review`); resztę pokazuje człowiekowi do
wskazania miejsca na nagraniu. „Pewne” = przeszło strażnika ORAZ: żadne słowo poprzedniego wersu
nie padło po początku cięcia (0,3 s zapasu), powtarzany wers nie zaczyna się tak jak inny w pobliżu
(to samo pierwsze słowo albo 2 z 3 pierwszych), cięcie najwyżej 8 s.

Sama długość pauzy nic nie mówi — pomiar na 11 nagraniach lektorów:

| pauza | n | 25% | mediana | 75% | 95% | max |
|---|---|---|---|---|---|---|
| między wersami | 167 | 0,41 s | 0,75 s | 1,15 s | 1,90 s | 3,38 s |
| w środku wersu | 100 | 0,18 s | 0,37 s | 0,63 s | 1,24 s | 1,99 s |

Ta sama próba (354 R, pomyłka + R + ponowne czytanie):

| | cięć | niebezpiecznych |
|---|---|---|
| bez strażnika | 160 | 65 |
| strażnik | 89 | 18 |
| **pewne** | **64** (48 dobrych, 16 częściowych — zostaje kawałek pomyłki) | **0** |
| do wskazania | 96 | — (decyduje człowiek) |
| bez propozycji | 194 | — (R bez znalezionego początku wersu; też do wskazania) |

0 na 64 to statystycznie „poniżej ok. 5%” (95%) — potrzeba wielu tysięcy prób, zanim automat
będzie mógł ciąć sam. Do tego czasu `punchAutoCut` zostaje wyłączone.
