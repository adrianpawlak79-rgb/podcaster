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
2. **Dwa fałszywe alarmy „dryf poziomu”** (waga „ustawienia”) — oba kilka sekund po serii R,
   czyli po długich pauzach z odliczaniem. Sprawdzić, czy pauza po R nie psuje odniesienia.
3. **Bez modelu mowy śledzenie jest słabe: 22% klatek z właściwym wersem, 45% ±1 wers.**
   Skutki: R cofa do złego wersu (3 z 3 źle), wycięcie zabiera 1,28 s dobrego czytania
   i zostawia 1,3 s pomyłek, znaczki trafiają w złe wersy. Najpierw sprawdzić na prawdziwym
   głosie (pkt „Czego nie sprawdza”); jeśli się potwierdzi — program bez modelu mowy nie
   powinien proponować R albo powinien cofać do wersu z `LineLog`, nie z licznika sylab.
4. **Jeden „słyszalny szew”** w przebiegu B przy stałym tle −70 dBFS — prawdopodobnie
   porównanie poziomów łapie końcówkę mowy przy szwie. Sprawdzić `Punch` (seams).
