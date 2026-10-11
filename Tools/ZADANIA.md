# Zadania gotowe do oddania (Gemini, Codex, ktokolwiek)

Każde zadanie da się wkleić w całości jako polecenie. Zasady pracy: `AGENTS.md`.

Dwa sposoby:
- **automat:** treść zadania jako zgłoszenie (issue) na GitHubie z etykietą `gemini` — Gemini zrobi je w GitHub Actions i otworzy PR (`.github/workflows/gemini.yml`). W automacie pomiń linijki o tworzeniu gałęzi, commitach i wypychaniu — to robi automat;
- **ręcznie:** wklej do Gemini CLI na swoim komputerze (wtedy wszystko jak niżej).

---

## 1. Lampka w prompterze: zamienione napisy

```
Przeczytaj AGENTS.md. Utwórz gałąź gemini/lampka-napisy od main.

Błąd: w Source/PrompterComponent.h, w drawHud(), lampka pokazuje dla
stanu 1 napis „POPRAW”, a dla stanu 2 „POWTÓRZ”. Źródło prawdy to
Source/LampLogic.h: state 0 spokój, 1 powtórz, 2 popraw, 3 stop. Ekran
główny (Lamp w Source/MainComponent.h) robi to dobrze.

Popraw tak, żeby stan 1 = „POWTÓRZ”, stan 2 = „POPRAW”, i żeby kolory
znaczyły to samo co na ekranie głównym. Zmień tylko to miejsce.

Logika wyświetlania siedzi w komponencie JUCE, więc testu w Tests/ do
tego nie ma — w opisie PR napisz wprost, że zmiana nie ma testu
automatycznego, i jak sprawdziłeś ją ręcznie.

Commit po polsku: co i dlaczego. Wypchnij gałąź i otwórz PR do main.
```

---

## 2. Instalator Windows (NSIS)

Uwaga: zależy od PR „Prompter: Vosk…” (gałąź `claude/prompter-vosk`). Zacznij po jego scaleniu albo od tej gałęzi.

```
Przeczytaj AGENTS.md. Utwórz gałąź gemini/instalator.

Cel: jeden plik Zielone-swiatlo-instalator.exe zbudowany NSIS-em
(makensis działa też na Linuksie), który instaluje do
%ProgramFiles%\Zielone swiatlo:
  - Zielone swiatlo.exe (z build-win, cel ZieloneSwiatlo),
  - libvosk.dll i trzy DLL MinGW, od których zależy
    (libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll),
  - folder vosk-model-small-pl-0.22\ (cały, bez zmian).
Dodaje skrót w menu Start i opcjonalnie na pulpicie, oraz deinstalator
widoczny w „Aplikacje i funkcje”. Interfejs instalatora po polsku.

Nie commituj żadnych .exe, .dll ani modelu (są w .gitignore).
Instalator ma brać je z katalogu podanego przy budowaniu, np.
  makensis -DPLIKI=C:\sciezka\do\plikow Tools/instalator/zielone-swiatlo.nsi

Program nie wymaga tych plików do uruchomienia — bez nich prompter
liczy sylaby (Source/VoskEngine.h). Ustawienia i nagrania leżą
w katalogu użytkownika, więc deinstalator ich NIE usuwa.

Dodaj Tools/instalator/zielone-swiatlo.nsi i krótką instrukcję
Tools/instalator/JAK-ZBUDOWAC.md. W opisie PR podaj rozmiar
zbudowanego instalatora i czy instalacja/deinstalacja była sprawdzona
na prawdziwym Windowsie (jeśli nie — napisz to wprost).
```

---

## 3. Nowy wygląd według prototypu (Codex / ChatGPT) — pięć kroków, pięć PR-ów

Wzór i decyzje: `Docs/projekt/README.md`. Rób kroki **po kolei**; następny dopiero po
scaleniu poprzedniego (wszystkie dotykają wspólnych plików).

Środowisko Codex — **Setup script** (żeby budować program i robić zrzuty `--preview-ui`):

```bash
sudo apt-get update -q
sudo apt-get install -y -q g++ cmake ninja-build espeak-ng ffmpeg python3-pip xvfb \
  libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libasound2-dev
python3 -m pip install --break-system-packages -q soundfile pyloudnorm
git clone -q --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git /opt/JUCE-7.0.12
```

Budowanie i zrzut: `cmake -G Ninja -B build -DZS_JUCE_PATH=/opt/JUCE-7.0.12 && cmake --build build --target ZieloneSwiatlo`,
potem `xvfb-run -a "build/ZieloneSwiatlo_artefacts/Release/Zielone swiatlo" --preview-ui zrzut.png recording`.

### 3.1 Fundament i ekran Nagranie

```
Przeczytaj AGENTS.md i Docs/projekt/README.md, obejrzyj Docs/projekt/ekran-2.png
i Docs/projekt/prototyp.html. Gałąź codex/wyglad-1-fundament od main.

1. Paleta, krój i miary z Docs/projekt/README.md: zmień Source/UiRules.h
   (kolory, rodzaje przycisków — dodaj trzeci rodzaj: okrągły „Nagraj”) i Source/Look.h
   (krój Segoe UI). Zaktualizuj Tests/wyglad-test.cpp tak, żeby pilnował NOWEJ palety
   (kontrast WCAG, kolor główny #39CF7B, brak kolorowania przycisków poza Look.h).
2. Wspólny szkielet ekranu: górny pasek, pasek 5 kroków ze strzałkami (Source/StepBar.h),
   treść + panel boczny, stopka — jak w prototypie.
3. Ekran Nagranie (MainComponent.h, PrompterComponent.h) według ekran-2.png, ale
   z lampką zamiast samego paska poziomu, z Esc (zatrzymaj i oceń) i R (powtórz wers)
   oraz prompterem idącym za głosem — patrz tabela „Działanie programu zostaje”.

Nie zmieniaj niczego w Source/DSP/, VoskEngine.h, Recorder.h. Zachowaj końce linii (CRLF
tam, gdzie są). Uruchom bash Tests/ci.sh — ma przejść. Dołącz do PR zrzuty
--preview-ui: recording, prompter-rec, small, i porównanie z ekran-2.png.
Opis PR po polsku: co zmienione, wynik testów, czego nie sprawdzono.
```

### 3.2 Przygotowanie — `codex/wyglad-2-przygotowanie`
Ekran główny i ustawienia sprzętu (`MainComponent.h`) oraz kreator (`WizardComponent.h`)
według `ekran-1.png`, na fundamencie z 3.1. Zrzuty: `small`, `settings`, `wizard`.

### 3.3 Odsłuch — `codex/wyglad-3-odsluch`
`ReviewComponent.h` według `ekran-3.png`: duży przebieg (wysokość z
`zs::ui::review::waveHeight`), odtwarzanie, przed/po, **zdanie oceny z TakeReview**
w panelu bocznym (nie pusty kafel). Zrzuty: `review`, `review-big` na `/tmp/mowa.wav`.

### 3.4 Montaż — `codex/wyglad-4-montaz`
Tryb edycji w `ReviewComponent.h` według `ekran-4.png`: przycięcie początku i końca
na osi (`CutEdit.h`). Bez przełączników miksu — panel boczny pokazuje, co dobrał
automat (`Decide.h`), zdaniem i liczbą.

### 3.5 Gotowe — `codex/wyglad-5-gotowe`
Ekran zapisu według `ekran-5.png`: nazwa pliku, format, długość; „Zapisz miks”,
„Wróć do montażu”, „Nagraj nowe podejście”.

---

## 4. Nagrywanie „punch and roll” — jedno ciągłe nagranie, bez składania

**Decyzja Adriana (11 października):** program nie pokazuje „podejść” ani plików do
składania. Jest jedno nagranie. Dobrze — jedziesz dalej. Pomyłka — człowiek decyduje, że
się cofa, a program sam robi resztę: cofa, odtwarza końcówkę, nagrywa dalej i nadpisuje
błąd. Na końcu jest jeden czysty plik. Tak pracują lektorzy audiobooków (Reaper, Audition),
tylko tam trzeba to ustawiać — tu ma działać od razu.

Zastępuje dawne zadania „lista podejść” i „najlepsze wersy z kilku podejść” (oba zakładały
składanie wielu plików, w którym użytkownik się gubi).

### Jak to wygląda dla człowieka
1. Czyta z promptera. Lampka zielona — czyta dalej.
2. Pomylił się (albo lampka mówi „powtórz”) — wciska **R** (albo pilot / przycisk).
3. Program **zatrzymuje zapis**, prompter wraca na **początek bieżącego wersu**
   (strzałka ↑ przed wznowieniem cofa o kolejny wers).
4. W słuchawkach **2–3 s nagrania przed tym miejscem** (pre-roll), żeby złapać ton i tempo;
   na ekranie odliczanie i podświetlony wers.
5. **Od punktu cięcia program nagrywa dalej**; wszystko, co było po nim, znika z nagrania.
6. Na końcu: jeden plik, szwy niesłyszalne. Nic do składania.

### Kroki (każdy osobny PR z testem)
1. **Czasy wersów w trakcie nagrania** — prompter zapisuje, kiedy zaczyna się każdy wers
   (źródło: słowa Vosk / sylaby / strzałki). Potrzebne do wyznaczenia punktu cięcia.
   Obok nagrania także `sesja_<data>.wersy.txt` (przyda się w Odsłuchu).
2. **Punkt cięcia:** początek wersu dosunięty do **najbliższej ciszy przed nim** (śledzenie
   myli się o 1–2 słowa, więc granica z samego śledzenia mogłaby ciąć w słowie).
   Test bez JUCE: syntetyczna mowa z przerwami, punkt ma wypaść w ciszy, nigdy w środku słowa.
3. **Zapis segmentami:** `Recorder` po R zamyka bieżący segment (na dysku, nic nie ginie),
   zapamiętuje punkt cięcia i zaczyna nowy segment. Przy zatrzymaniu skleja segmenty w jeden
   plik: segment 1 do punktu cięcia + segment 2 … Odrzucone końcówki zostają w ukrytym
   podkatalogu (cofnięcie pomyłki, bezpieczeństwo), użytkownik ich nie widzi.
4. **Szew:** krótkie przenikanie (10–30 ms) w ciszy, ten sam poziom tła po obu stronach.
   Wykorzystać `MontageRender.h`, ale najpierw naprawić znany błąd z
   `Tests/znane-bledy.txt` (`montage-test` 3 z 19 — cięcie w środku mowy).
   Test: trzask i stopień tła na szwie jak dziś w `montage-test`.
5. **Pre-roll w słuchawkach** + odliczanie. Bez słuchawek (kreator, krok „Słuchawki”
   wykrył przesłuch) — pre-roll wyciszony, zostaje samo odliczanie, żeby głośnik nie wszedł
   do mikrofonu.
6. **Lampka po cięciu** zaczyna od nowa dla nowego segmentu; ocena po nagraniu
   (`TakeReview`) liczy już z pliku sklejonego.

### Znaczek przy każdym przeczytanym wersie (pomysł Adriana)
W prompterze, przy wersach już przeczytanych, mały znak — żeby od razu było wiadomo,
czy jechać dalej, czy cofnąć:
- **✓ zielony** — w czasie tego wersu lampka nic nie zgłosiła;
- **! żółty** — coś było, ale miks to poprawi (plozja, mlask, odsunięcie) — można jechać dalej;
- **✗ czerwony** — rzecz nie do naprawienia (przester, ucięcie, brak sygnału) — podpowiedź „R”.

Skąd dane: zdarzenia lampki mają czas (`tw::Event`), wersy mają czas z kroku 1 — wystarczy
je dopasować. Ten sam podział co w ocenie po nagraniu (`TakeReview`: naprawialne / do
powtórki), więc w prompterze i na ekranie Odsłuch znaczek znaczy to samo.
Po cofnięciu (R) znaczki od punktu cięcia znikają i liczą się od nowa.

„Dobrze” znaczy **technicznie dobrze**, nie „ładnie przeczytane”. Później, osobno: znak
„?” przy wersie, w którym rozpoznawanie mowy nie znalazło większości słów (możliwe
przejęzyczenie) — dopiero po pomiarze, ile fałszywych „?” daje na nagraniach Adriana
(śledzenie trafia dziś w ≤1 słowo w 66% chwil).

Test bez JUCE: zdarzenia lampki + czasy wersów → właściwy znak dla każdego wersu,
także gdy zdarzenie wypada na granicy dwóch wersów.

### Do ustalenia przy budowie
- czy R zawsze robi punch-and-roll, czy zostaje też dzisiejsze „cofnij tekst, nagrywaj
  dalej” (wtedy jako opcja w Ustawieniach → Prompter);
- długość pre-rollu (domyślnie 2 s), w Ustawieniach.

## 5. „Nagraj całość, popraw na końcu” — drugi tryb, dla tych, co nie chcą przerywać

Pomysł Adriana: zamiast cofać się w trakcie (zadanie 4), czytasz cały tekst do końca,
a potem program prowadzi przez poprawki tylko tego, co wyszło gorzej. Oba tryby mają działać
na tym samym mechanizmie i dawać ten sam wynik: jeden czysty plik.

### Jak to wygląda
1. Nagrywasz całość, nie przerywasz (znaczki ✓ / ! / ✗ i tak się zapalają).
2. Po zatrzymaniu, na ekranie Odsłuch: **tekst z znaczkami** i przycisk
   **„Popraw zaznaczone (3)”** — liczba wersów z ✗ (domyślnie) albo z ✗ i ! (do wyboru).
   Każdy wers da się kliknąć i posłuchać; można odznaczyć „ten zostaje” albo dodać
   wers, który nie podoba się z innego powodu.
3. „Popraw zaznaczone”: prompter pokazuje **pierwszy wers do poprawki** z jednym wersem
   kontekstu przed nim; w słuchawkach 2 s nagrania przed tym wersem, odliczanie, czytasz
   **tylko ten wers**. Lampka ocenia go od razu — ✓ idzie dalej, ✗ proponuje jeszcze raz.
4. Następny zaznaczony wers… aż do końca listy.
5. Program **wstawia nowe wersy w miejsce starych** (punch-in w środku nagrania),
   szwy w ciszy przed i po wersie. Jeden plik, stary w ukrytej kopii.

### Kroki (po zadaniu 4 — korzysta z jego czasów wersów, punktów w ciszy i szwów)
1. Ekran tekstu ze znaczkami na Odsłuchu (dane: zdarzenia lampki + czasy wersów).
2. Lista „do poprawki”: domyślnie ✗, człowiek dodaje i odejmuje.
3. Nagrywanie pojedynczego wersu z pre-rollem i natychmiastową oceną.
4. Wstawienie w środek: **dwa szwy** (przed i po), oba w ciszy; długość nowego wersu
   może być inna niż starego — reszta nagrania przesuwa się, nic nie nachodzi.
   Test: wstawienie krótszego i dłuższego wersu w syntetyczną mowę → brak trzasku,
   ten sam poziom tła, długość wyniku = stara − stary wers + nowy wers.
5. Wyrównanie poziomu i barwy wstawki do otoczenia (pomiary: `VoiceTone.h`, LUFS) —
   poprawkę nagrywa się minutę albo godzinę później i mikrofon mógł się przesunąć.

Ryzyko: intonacja na szwie (koniec zdania przed wstawką brzmi „na zamknięcie”, wstawka
„na otwarcie”). Test tego nie złapie — decyduje odsłuch; dlatego wstawka zawsze od początku
wersu do jego końca, nie w połowie.

## 6. Ekran „Ustawienia” (zakładki)

Decyzja Adriana: zamiast rozrzuconych opcji jeden ekran ustawień. Przycisk w miejscu
„Mikrofon i sprzęt”, zakładki po lewej. Podział pracy: Claude najpierw robi to, co pod
spodem (zapis w `Settings.h`, przekazanie do miksu, testy), Codex potem wygląd (po kroku 3.5).

| Zakładka | Zawartość |
|---|---|
| **Sprzęt** | karta, wejście z mikrofonem, słuchawki, częstotliwość, test słuchawek; **model mikrofonu** z wbudowanej tabeli (SM7B, SM58, RE20, PodMic, MV7, NT1, AT2020, Yeti… albo „dynamiczny / pojemnościowy / USB / nie wiem”) — na razie tylko rada o wzmocnieniu dla cichych dynamicznych; „Sprawdź sprzęt” z ostatnim wynikiem i datą |
| **Nagrania** | folder sesji, „Otwórz folder”; **zapis miksu domyślnie WAV + MP3** (WAV 24-bit, MP3 192 kb/s; każdy da się wyłączyć) |
| **Prompter** | wielkość tekstu, lustro, śledzenie (słowa / sylaby / wyłączone), klawisze i pilot prezentera (PageUp/PageDown) |
| **Miks** | cel: Podcast −16 / YouTube −14 / bez zmiany głośności (`Decide::Destination` już to umie); sufit szczytu; barwa: automatyczna / stała jak dawniej / później „jak w moim miksie” |
| **Lampka** | wyciszone rodzaje alarmów z możliwością przywrócenia |
| **O programie** | wersja, licencje (JUCE, Vosk, MP3), gdzie leżą ustawienia i nagrania |

### MP3 — skąd koder

JUCE nie ma własnego kodera MP3 (jego `LAMEEncoderAudioFormat` wywołuje zewnętrzny `lame.exe`).
Możliwości:
1. **Windows Media Foundation** — koder MP3 jest częścią Windows 8/10/11, nic nie dokładamy do
   paczki; kod tylko dla Windows (~150–200 linii), na Linuksie test bez kodowania. **Zalecane.**
2. LAME jako osobna DLL obok .exe (jak Vosk) — wyjątek od zasad, licencja LGPL.
3. Własny koder od zera — za duży na zysk.

Patenty MP3 wygasły (2017), więc użycie jest wolne.
Test: zakodować `/tmp/mowa.wav`, zdekodować tym samym API i sprawdzić długość, głośność (LUFS)
i szczyt po dekodowaniu względem WAV (mp3 potrafi podnieść szczyt — sufit miksu −1 dBTP jest
właśnie po to).

## 8. Tryb „Wiersz z podkładem” (muzyka)

Pomysł Adriana (11 października): oprócz podcastu nagrywanie wiersza pod muzykę. Na ekranie
Przygotowanie wybór **„Podcast” / „Wiersz z podkładem”** — ten sam, który był w prototypie
i który pominięto w 3.2, bo program nic z nim nie robił. Teraz dostaje znaczenie.

### Jak to wygląda dla człowieka
1. **Przygotowanie:** wybór trybu; „Wczytaj podkład” (WAV, FLAC, MP3); suwak **głośności
   podkładu**; opcjonalnie „głos wchodzi od” (np. 8 s wstępu muzyki).
2. **Nagranie:** podkład gra w słuchawkach od początku, prompter jak zwykle. Głos zapisuje się
   na **osobnej ścieżce**, równo z muzyką. Bez słuchawek — ostrzeżenie (muzyka z głośników
   weszłaby do mikrofonu; kreator ma już krok „Słuchawki”, który to mierzy).
3. **R przy muzyce = nadpisanie, nie wycięcie.** Muzyka jest ciągła, więc czasu wyciąć nie
   można — głos rozjechałby się z podkładem. Zamiast tego: podkład cofa się 2 s przed początek
   wersu (pre-roll), program nagrywa głos od początku wersu, a stary głos od tego miejsca
   znika. Tak działa punch-in w programach studyjnych. Wspólne z zad. 4: czasy wersów (#24),
   punkt w ciszy (#25), szew (#27).
4. **Miks:** głos przez ten sam łańcuch co podcast; podkład z głośnością z suwaka i
   **automatycznym ściszaniem pod głosem** (ile dB — ustawienie); całość do LUFS celu;
   zapis WAV + MP3 (zad. 6). Na odsłuchu suwak podkładu działa na żywo.
5. Zapis także osobno: sam głos (do własnego miksu w Studio One).

### Kroki techniczne (każdy osobny PR z testem)
1. Wybór trybu i wczytanie podkładu (MP3 na Windowsie przez Media Foundation — ten sam
   mechanizm co zapis MP3 z zad. 6), zapamiętane w ustawieniach sesji.
2. Odtwarzanie podkładu w słuchawkach w trakcie nagrania, z **kompensacją opóźnienia karty**
   (wejście + wyjście; zmierzyć pętlą jak w kreatorze, krok „Słuchawki”), żeby głos leżał
   równo z muzyką. Test: syntetyczna pętla z opóźnieniem → wyrównanie co do 1 ms.
3. Punch-in z nadpisaniem (pre-roll podkładu, nagranie od punktu w ciszy, przenikanie).
4. Miks głos + podkład: ściszanie podkładu pod głosem (sidechain), głośność całości.
5. Ekrany: Przygotowanie (podkład, suwak), Odsłuch (suwak podkładu na żywo) — dla Codexa.

### Ustalone z Adrianem (11 października)
- **Podkład ściszany pod głosem automatycznie, w miksie** (jak „auto-ducking” w Premiere Pro),
  i ma to być „inteligentne”. Czyli nie stałe −8 dB, tylko: program mierzy głośność głosu
  i podkładu, pod mową trzyma muzykę tyle niżej, żeby głos był wyraźny (cel do ustalenia
  pomiarem, punkt wyjścia ok. 15 LU pod głosem), w dłuższych przerwach między strofami
  muzyka wraca płynnie do góry, zmiany łagodne (narastanie/opadanie rzędu 0,3 s / 1 s),
  krótkie pauzy między słowami jej nie ruszają. Suwak głośności podkładu dalej działa —
  przesuwa całość. Test: syntetyczny głos + szum muzyczny → zmierzona różnica pod mową
  i powrót w przerwie.
- **Podkład ma być odpowiednio długi.** Program przed nagraniem porównuje długość podkładu
  z przewidywanym czasem czytania (z tekstu, jak szacunek w prompterze) i ostrzega, gdy
  jest za krótki. Gdy jest dłuższy — w miksie koniec podkładu płynnie wyciszony kilka
  sekund po ostatnim słowie.
- **„Wiersz” bez podkładu = zwykły tryb podcastu.** Wybór trybu sprowadza się do pytania
  „czy jest podkład”.

Uwaga do pomocy w programie: prawa do podkładu — przy publikacji potrzebna muzyka z licencją.

---

# Zadania nieprogramistyczne

## 7. Film z instrukcją

Pomysł Adriana: krótki film, który pokazuje, jak używać programu. Większość da się zrobić
automatycznie — obraz programu nagrywa się bez człowieka (tak jak dziś zrzuty `--preview-ui`).

### Co powstaje
- **„Jak zacząć” (3–4 min):** kreator sprzętu → przygotowanie (karta, wejście, tekst) →
  nagranie z prompterem i lampką → cofnięcie klawiszem R → odsłuch i werdykt → zapis.
- **Krótkie klipy (20–40 s) po jednej funkcji:** lampka, prompter idący za głosem,
  R / punch and roll, „popraw zaznaczone”, zapis WAV + MP3. Nadają się też na stronę
  i do mediów społecznościowych (test z opinii ChatGPT/Gemini: pokazać lampkę w działaniu).

### Jak (kroki)
1. **Scenariusz** — pisze AI, zatwierdza Adrian: co widać, co mówi lektor, ile sekund.
2. **Tryb pokazowy w programie:** `--demo nagranie.wav tekst.txt` — plik WAV podawany
   zamiast mikrofonu, w czasie rzeczywistym. Prompter idzie za głosem, lampka reaguje,
   wszystko powtarzalne co do sekundy. To jedyna część programistyczna (osobny PR z testem);
   przyda się też do testów i do pokazywania programu bez mikrofonu.
3. **Nagranie obrazu automatycznie:** program pod Xvfb + `ffmpeg -f x11grab`
   (działa w kontenerze, bez człowieka), 1920 × 1080, kursor i kliknięcia ze skryptu.
4. **Głos lektora** — do wyboru:
   - **Adrian czyta scenariusz w Zielonym świetle** (najlepiej: prawdziwy głos, a przy okazji
     pokaz programu „od środka”),
   - albo syntezator offline (np. Piper; sprawdzić licencję wybranego głosu polskiego —
     espeak brzmi zbyt robotycznie).
5. **Złożenie:** ffmpeg — obraz + głos + napisy (SRT po polsku), powiększenia na lampkę
   i prompter, muzyka bez praw autorskich albo bez muzyki.
6. **Sprawdzenie:** czy każde zdanie lektora zgadza się z tym, co widać (to AI może
   porównać ze scenariuszem), i odsłuch przez Adriana.

### Kiedy
Po zadaniach 4 (punch and roll) i 3.5 / 6 (ekran zapisu, ustawienia) — inaczej film pokaże
ekrany, które za chwilę się zmienią. Wcześniej można zrobić krok 2 (tryb pokazowy), bo nie
zależy od wyglądu.
