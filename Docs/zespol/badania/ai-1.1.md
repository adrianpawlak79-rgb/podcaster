# Badanie AI do wersji 1.1: whisper.cpp i odszumianie neuronowe (28.11.2026)

Wszystkie liczby pochodzą z pomiaru w kontenerze (4 vCPU x86-64 z AVX2, bez GPU, 15 GB). Część pomiarów
czasu szła równolegle z innym buildem i jest zaznaczona. Skrypty i surowe wyniki są w `/tmp/badania/`
(poza repo).

## Materiał
- **Syntezator:** 129 s polskiej mowy espeak-ng. Skrypt ma 23 zdania. W nagraniu dodałem 34 odcinki:
  6× „yyy”/„eee”, 3 falstarty (pół zdania i całe od nowa), 1 przejęzyczenie z powtórką i 1 zdanie
  powiedziane dwa razy. Czas każdego odcinka jest znany co do próbki.
- **Prawdziwy głos:** 6 wierszy z Wolnych Lektur (audiobooki CC BY-SA 3.0, tekst w domenie publicznej),
  razem 7,2 min, 5 lektorów, w tym 2 kobiety, ok. 600 słów wzorca. W 3 wiersze wstawiłem po 3 falstarty
  (1,8 s początku wersu powtórzone po pauzie), razem 9. W `/tmp/mowa/` jest tylko espeak, a w
  `Tests/*.wav` nie ma mowy z tekstem.
- **Odszumianie:** 30 s mowy (espeak, kobieta K i mężczyzna M z Wolnych Lektur) + „telewizor”
  (angielski espeak, pasmo 200–5000 Hz, −25 dB) + szum różowy −45 dB; wariant B z pogłosem (RT60 0,45 s).

## Badanie 1: whisper.cpp po polsku (po nagraniu)

whisper.cpp v1.9.6 (commit z 28.11), `whisper-cli -l pl -ml 1 -oj`, beam 5 (domyślnie).

| model | plik | WER espeak | WER prawdziwy głos (6 wierszy) | czas 129 s espeak, 4 wątki | RTF | pamięć (RSS) |
|---|---|---|---|---|---|---|
| tiny | 78 MB | 74 % | nie mierzone (za słaby) | 245 s* (pętle) | 1,9* | 280 MB |
| base | 148 MB | 61 % | 29 % | 15 s | 0,12 | 390 MB |
| small | 488 MB | 35 % | 15 % | 41 s | 0,32 | 855 MB |
| small q5_0 | 175 MB | 36 % | nie mierzone | 442 s* | 3,4* | 560 MB |
| large-v3-turbo q5_0 | 574 MB | 12 % | **9 %** (0–17 % na wiersz) | 123 s | 0,96 | 915 MB |

\* Pomiar przy równoległym buildzie. Dekoder ggml przy 4 wątkach i zajętym CPU zwalnia nawet 50 razy:
base miał 123 ms zamiast 2,5 ms na krok, bo wątki czekają w pętli. W programie liczyć na `-t` = liczba
rdzeni − 1. Czasy tiny i small q5_0 trzeba powtórzyć. Prawdziwy głos przy 2 wątkach: base RTF ≈ 0,2,
small ≈ 0,5, turbo ≈ 2,0–2,6.

WER na espeaku jest 2–4 razy gorszy niż na prawdziwym głosie. Espeak po polsku jest mało zrozumiały
(„podsastu”, „błośność”). **Ranking modeli przenosi się na prawdziwy głos, a wartości bezwzględne nie.**

**Znaczniki czasu słów (espeak, początek zdania lub powtórki względem prawdy):**

| model | mediana błędu | 90. percentyl |
|---|---|---|
| base | 0,40 s | 0,84 s |
| small | 0,26 s | 0,39 s |
| turbo q5 | **0,05 s** | 0,77 s |

Prompt „Yyy, eee…” pogarsza wynik: znaczniki rosną do 0,5–0,8 s, small wpadł w pętlę (przerwałem po
15 min). **Nie używać promptu.** Do cięcia z dokładnością ±50 ms znaczniki Whispera trzeba
dosunąć do ciszy naszym DSP (granica słowa = najbliższa pauza w energii).

**„yyy”/„eee” (6 wstawek w espeaku):** jako tekst Whisper zapisał 1–2 z 6 przy każdym modelu. Przez
dopasowanie do skryptu (słowo spoza skryptu w danym miejscu, często „i”): turbo 4/6 przy 3 fałszywych,
small 3/6 przy 1. „Luki” (jest energia, nie ma słowa) nie działają: 0–2/6, bo Whisper rozciąga sąsiednie
słowa na wypełniacz. Espeak mówi „yyy” inaczej niż człowiek, więc to tylko orientacja.

### Powtórki: prototyp `/tmp/badania/ocena.py` (funkcja `align_script`)
Program ma tekst skryptu. Dopasowuję słowa Whispera do skryptu programowaniem dynamicznym: zgodne słowo
(podobieństwo znaków ≥ 0,6) kosztuje 0, wstawka 1 (wypełniacz 0,6), pominięte słowo 1, a **skok wstecz**
do początku bieżącego albo jednego z 2 poprzednich zdań 1,5.

Słowo, którego pozycję w skrypcie później pokrywa ponowne czytanie po skoku, oznaczam jako „do wycięcia”
(zostaje ostatnie podejście, jak przy R).

| materiał | model | wykryte powtórki | fałszywe alarmy (słowa) |
|---|---|---|---|
| espeak (5 powtórek) | base / small / **turbo** | 3 / 3 / **5 z 5** | 3/157, 0/165, **0/174** |
| prawdziwy głos (9 falstartów) | base / small / turbo | 4 / 5 / **2 z 9** | 5 / 1 / 1 z ok. 340 |
| prawdziwy głos, small z `-mc 0` | small | **6 z 9** | 2 z ok. 335 |

**Najważniejsze ryzyko:** na prawdziwym głosie Whisper **wygładza** falstarty. Turbo zapisał „ej
niegorzkie plony pól” raz, choć padło dwa razy, i rozciągnął znaczniki czasu na oba podejścia. Im lepszy
model językowy, tym chętniej „poprawia” mówcę. `-mc 0` (bez kontekstu poprzedniego okna) częściowo
pomaga. Do potwierdzenia jest wykrywanie anomalii czasu: słowo trwające 2× dłużej niż wynika z sylab
(mamy licznik sylab) oznacza miejsce do sprawdzenia. Podpowiedzi w montażu powinny więc mówić „sprawdź
tu”, a nie ciąć same.

### Licencje i integracja
- **Licencje:** kod whisper.cpp jest na MIT. Wagi Whispera (OpenAI) też są na MIT. Kod i model wolno
  dołączyć do instalatora.
- **MinGW:** cross-kompilacja MinGW-w64 GCC 13 (`mingw-toolchain.cmake` z repo) **działa** po dwóch
  poprawkach: `-D_WIN32_WINNT=0x0601` (stare nagłówki MinGW nie mają `THREAD_POWER_THROTTLING_STATE`)
  i jawne `-DGGML_AVX=ON -DGGML_FMA=ON -DGGML_F16C=ON` przy `GGML_NATIVE=OFF`.
- **Wynik builda:** `libwhisper.a` 0,8 MB + ggml 2,8 MB; `whisper-cli.exe` statycznie 4,0 MB po strip.
  Na Windows nie uruchomiłem (w kontenerze nie ma wine).
- **AGENTS.md** dopuszcza tylko JUCE (wyjątek: Vosk jako DLL). Proponuję tę samą drogę: osobny proces
  pomocniczy (`zs-tekst.exe` z whisper.cpp) albo DLL. Obok przemawia za tym ~0,9 GB RAM i minuty
  liczenia. Awaria modelu nie kładzie wtedy okna, a kolejkę łatwo przerwać.
- **Rozmiar:** do instalatora tylko kod (+4 MB). Model small (488 MB) albo turbo q5 (574 MB) pobierany
  na żądanie przy pierwszym użyciu, jak model Vosk. Pliki z huggingface.co/ggerganov/whisper.cpp.

## Badanie 2: odszumianie neuronowe a nasz tor

Kandydaci:
- **RNNoise** (xiph, BSD-3, czyste C, model wkompilowany): exe MinGW 3,6 MB, czas 0,9 s na 30 s.
- **DeepFilterNet3** (MIT/Apache-2.0, Rust): działa bez Pythona (binarka `deep-filter` i API C
  `df_create`/`df_process_frame`). DLL dla Windows (`x86_64-pc-windows-gnu`) zbudowała się: 29 MB po
  strip, z modelem w środku. Do budowania potrzebny Rust. Czas 3,7 s na 30 s.
- **Nasz tor:** `zs::TakeReview::run()` z domyślnym „Podcast”, 2,0 s na 30 s.

**Metoda.** Wyjście rozkładam w STFT, w blokach 0,4 s i w każdym prążku, metodą najmniejszych kwadratów
na składowe od znanej mowy, TV i szumu. To, co zostaje, to „artefakty”: zniekształcenia, ale też zmiany
dynamiki od kompresora. Ramki mowy i pauz wyznaczam z czystej mowy. Poziomy podaję w dB względem
składowej mowy. STOI (pystoi) liczę względem czystej mowy. PESQ (pip) nie zbudował się.

| scenariusz | tor | TV w trakcie mowy | tło w pauzach | STOI |
|---|---|---|---|---|
| K (kobieta) + TV + szum | wejście | −25,8 | −25,7 | 0,983 |
| | nasz tor | −23,4 | −24,1 | 0,971 |
| | RNNoise → nasz | −25,0 | **−49,4** | 0,955 |
| | DeepFilterNet → nasz | −25,7 | −34,4 | 0,945 |
| M (mężczyzna) + TV + szum + pogłos | wejście | −25,2 | −25,3 | 0,986 |
| | nasz tor | −21,9 | −22,6 | 0,956 |
| | RNNoise → nasz | −23,4 | **−35,7** | 0,944 |
| | DeepFilterNet → nasz | −22,8 | −27,8 | 0,930 |
| espeak + TV + szum | nasz tor | −24,4 | −25,3 | 0,966 |
| | RNNoise → nasz | −23,4 | −31,4 | **0,738** |
| | DeepFilterNet → nasz | −25,6 | −27,9 | 0,937 |

Pozostałe warianty (K z pogłosem, M bez pogłosu, espeak z pogłosem) są w `/tmp/badania/odszum.txt`.
Wnioski są te same.

**Wnioski z pomiaru:**
1. **Żaden tor nie usuwa telewizora w trakcie mowy.** Różnice mieszczą się w ±3 dB. TV to też mowa,
   a RNNoise i DeepFilterNet uczono, że mowa to sygnał. Nasz tor po kompresji podnosi TV o 2–3 dB
   względem głosu.
2. **W pauzach RNNoise jest bardzo skuteczny:** tło o 12–25 dB niższe niż przy samym naszym torze na
   prawdziwym głosie; DeepFilterNet daje 5–10 dB. Nasz tor sam nie ścisza TV w pauzach (−24 przy −26 na
   wejściu): bramka nie rozpoznaje mowy z TV jako tła.
3. **Zniekształcenie:** na prawdziwym głosie RNNoise prawie nic nie psuje (STOI surowego 0,96–0,98), na
   espeaku niszczy mowę (0,76), bo syntetyczny głos uznaje za szum. Przestroga przed syntezatorem.
4. Pogłos (M, B) zmniejsza zysk RNNoise w pauzach z −50 do −36 dB. Pogłosu nikt nie usuwa.

**Pliki do odsłuchu:** `Docs/zespol/badania/odsluch/*.wav`, po 15 s (od 5. sekundy), 16 bit, 48 kHz.
- `kobieta-tv-{1-wejscie,2-nasz-tor,3-rnnoise-nasz-tor,4-deepfilternet-nasz-tor}.wav`
- `mezczyzna-tv-poglos-{1..4}-*.wav`

Wejście jest cichsze niż wyjście (to drugie ma −16 LUFS). Uwaga: to fragmenty nagrań CC BY-SA
(Wolne Lektury). Przy commicie trzeba dopisać źródło albo ich nie commitować.

### Własna mała sieć
Zmierzyłem trening (PyTorch CPU, 2 wątki) sieci w stylu RNNoise 2017: 42 cechy, GRU 48/96/96, 145 tys.
parametrów. Daje 34 tys. ramek 10 ms na sekundę, czyli **0,3 h na epokę przy 100 h danych**. Sieć
większa (1 mln parametrów) potrzebuje 0,7 h. Kilkadziesiąt epok to **1–2 doby na tym kontenerze, bez
GPU, czyli wykonalne.**

Wąskie gardło to dane i cel, nie moc. Czysty polski głos jest (Wolne Lektury, dziesiątki godzin), szumy
trzeba zebrać. Do usuwania **telewizora** RNNoise na własnych danych nie wystarczy: potrzebna sieć
„spersonalizowana”, warunkowana próbką głosu Adriana z kreatora. To osobny projekt na kilka tygodni z
ryzykiem porażki. Inferencję GRU (ok. 300 linii C++) da się napisać samemu w `Source/DSP/`.

## Rekomendacja
1. **Rozpoznawanie mowy po nagraniu:** whisper.cpp jako osobny proces albo DLL. Domyślnie model
   **small** (488 MB, WER 15 % na prawdziwym głosie, ok. 0,3–0,5× czasu nagrania na 4 rdzeniach).
   Opcja „dokładniej”: **large-v3-turbo q5_0** (574 MB, WER 9 %, ok. 1× czasu nagrania). Model
   pobierany przy pierwszym użyciu, w instalatorze +4 MB kodu. Licencje MIT.
2. **Podpowiedzi powtórek:** dopasowanie do skryptu ze skokiem wstecz (prototyp gotowy). Pokazywać
   „sprawdź tu”, nie ciąć automatycznie. Uruchamiać z `-mc 0`, bez promptu. Wypełniacze wykrywać jako
   słowa spoza skryptu.
3. **Odszumianie:** RNNoise (BSD-3, +3,6 MB, czyste C, ~1 s na 30 s) jako opcjonalny moduł przed torem,
   który wycisza pauzy. Najpierw odsłuch Adriana. Telewizora w trakcie mowy nie usuwa żaden kandydat.
   DeepFilterNet odradzam na teraz: 29 MB i Rust w buildzie, a w pauzach wypadł słabiej niż RNNoise.
4. **Zasady repo:** obie biblioteki to zewnętrzny kod. Wymaga to decyzji Adriana, jak przy Vosku.
   Alternatywa dla RNNoise to własna inferencja GRU z wagami RNNoise (BSD pozwala) w `Source/DSP/`.

## Ryzyka
- Whisper wygładza falstarty i „yyy”: na prawdziwym głosie turbo wykrył 2/9 falstartów. Lepszy WER nie
  oznacza lepszego wykrywania powtórek.
- Czas liczenia silnie zależy od obciążenia CPU (pętle wątków ggml). Na słabym laptopie turbo może liczyć
  2–3× dłużej niż trwa nagranie.
- Halucynacje i pętle przy promptach i na tiny.
- RNNoise może „wycinać” ciche końcówki słów i oddechy. Trzeba sprawdzić łącznie z naszym modułem
  oddechów i bramką.
- Kwantyzacja q5 była u nas wolniejsza niż f16 (pomiar przy obciążeniu). Trzeba powtórzyć na Windows.

## Do potwierdzenia na prawdziwym głosie Adriana (SSL 2)
- WER small i turbo na 10–15 min jego nagrania z prawdziwymi pomyłkami, „yyy” i R, przy tekście skryptu.
- Czy prototyp powtórek trafia w jego falstarty: liczba trafionych i fałszywych na tej sesji.
- Czas small i turbo na jego laptopie (`-t` = rdzenie − 1) i czy pobieranie 0,5 GB jest do przyjęcia.
- Odsłuch RNNoise przed torem na nagraniu z prawdziwym telewizorem w pokoju: pauzy, końcówki słów,
  oddechy.
- Uruchomienie `whisper-cli.exe` i `rn.exe` z MinGW na Windows. Zbudowane, ale nieuruchomione.
