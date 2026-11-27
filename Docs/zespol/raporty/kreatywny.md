# Raport: dyrektor kreatywny (marka, teksty, pierwsze wrażenie)

Data: 27.11.2026. Tylko diagnoza, bez zmian w kodzie. Zrzuty: `Docs/zespol/raporty/zrzuty/kreatywny-*.png` (Xvfb :107, `--preview-ui`).

## 1. Ocena gotowości: 6/10
Ton jest ciepły i prosty (dobry kierunek: „Co dziś nagrywamy?”, kreator mówi „Ty”), ale ta sama rzecz ma 3–4 nazwy, w interfejsie zostały żargon i odsyłacze do nieistniejących ekranów, a nazwa programu w oknie i na pasku zadań nie ma polskich znaków.

## 2. Blokery wydania
1. **Nazwa programu bez polskich znaków.** `Source/Main.cpp:12` `getApplicationName()` zwraca „Zielone swiatlo” (pasek tytułu, pasek zadań Windows, Alt+Tab), `Main.cpp:128` splash „Zielone swiatlo”, `Installer/*.nsi:52` plik `Zielone swiatlo.exe`. Instalator (`NAZWA` w .nsi:30) i nagłówek w oknie mają „Zielone światło”, więc marka jest w dwóch wersjach. **Propozycja:** `juce::String::fromUTF8("Zielone światło")` w nazwie aplikacji i splashu; nazwę pliku .exe można zostawić ASCII (bezpieczniej), ale skrót w menu Start i DisplayName z „ś”.
2. **Odsyłacz do nieistniejącego miejsca „Mikrofon i sprzęt”.** `ReviewComponent.h:1128`, `ReviewComponent.h:1967`, `DSP/TakeReview.h:731`. W programie ten panel nazywa się „Ustawienia” (`MainComponent.h:372`). Adrian już raz nie znalazł przycisku (#140), to ten sam typ błędu. **Propozycja:** wszędzie „Ustawienia”.
3. **Ucięte teksty na ekranie startowym** (`kreatywny-witaj.png`): etykieta „Promp…” (`StartComponent.h:135`) oraz linijka „Mikrofon: nie wybrany · sprzęt niesp…” przykryta przyciskiem „Sprawdź sprzęt”. Pierwszy ekran, który widzi użytkownik. **Propozycja:** etykieta „Tekst:” albo usunąć (opcje mówią same za siebie), linijkę skrócić do „Mikrofon: nie wybrany” i dać jej miejsce.
4. **Niespójne nazwy tej samej rzeczy** (słowniczek niżej). Najgroźniejsze: „Nagraj poprawkę” / „Popraw fragment” / „Poprawki po R” to trzy różne funkcje z tym samym słowem; „sesja” oznacza raz folder, raz limit 60 min, raz nagranie.

## 3. Ważne, ale nie blokuje
- **Żargon:** „Poprawki po R” (`ReviewComponent.h:2293,2308,1371`), „punch”, „dBFS” i „dBTP” na ekranie Gotowe (`ReviewComponent.h:1177`, zrzut `kreatywny-done.png`: „-19.0 LUFS · szczyt -1.3 dBTP”, do tego minus z łącznika zamiast „−” i kropka dziesiętna zamiast przecinka), „Przydźwięk”, „Przesłuch słuchawek” w kreatorze (`WizardComponent.h:265,270`), „wzmocnienie”.
- **Za długie podpowiedzi:** tooltip `PrompterComponent.h:607` (3 zdania), opis podkładu `ReviewComponent.h:1385` (cały akapit), panel „Do sprawdzenia” (`ReviewComponent.h:1371–1388`) to instrukcja obsługi zamiast jednego zdania.
- **Przełącznik „słychać: PO miksie”** (zrzut `kreatywny-review.png`) – mała litera, wersaliki w środku. Lepiej dwa stany: „Przed miksem” / „Po miksie”.
- **Ekran Gotowe:** nazwa pliku „t2__miks-2.wav” (podwójne podkreślenie) i „MP3 nie powstał” bez wyjaśnienia, co zrobić. Brak gratulacji – to moment nagrody.
- **Wielkie litery w etykietach** („GOTOWY”, „SŁUCHAM”, „MIKROFON”, „DO POWTÓRZENIA (nie da się naprawić miksem)” `ReviewComponent.h:1379`) – krzyczą; zostawić tylko małe nadtytuły typu „03 / MIKS”.
- **Ikona kroku „Odsłuch i montaż”** wygląda jak „(0” (słuchawka + cyfra?) na wszystkich zrzutach – do sprawdzenia przez projektanta.

## 4. Drobne (polszczyzna i typografia)
- `PrompterComponent.h:1807` „zeby czytac” → „żeby czytać” (brak polskich znaków w tekście dla użytkownika).
- `StartComponent.h:148` „Po Starcie” → „Po kliknięciu Start” (wielka litera w środku zdania).
- `MainComponent.h:158/160` „GOTOWY”/„SŁUCHAM” – lampka mówi w rodzaju męskim; „Gotowe”/„Słucham” albo bezosobowo.
- `WizardComponent.h:404` „jak sam mówiłeś” – zakłada mężczyznę; „jak mówisz na co dzień”.
- `ReviewComponent.h:1549` „–” w zakresie czasu OK (półpauza), ale w skrótach klawiszowych używana jest pauza „—” (`ReviewComponent.h:1063,1395`) – można zostać, byle konsekwentnie; liczby: „0.0 s” → „0,0 s”, „18.8 s” → „18,8 s” (zrzut review).
- Cudzysłowy „” stosowane poprawnie prawie wszędzie (dobra robota poprzedników). Wielokropek „…” poprawny.
- `MainComponent.h:462,1875` „--- wyciszone w tej sesji” – trzy łączniki w tekście widocznym w uwagach.
- `ReviewComponent.h:2292` „Wycięto 0 fragmentów. Obie wersje pozostają…” – komunikat o niczym; lepiej nic nie mówić albo „Nic nie wycięto, obie wersje zostały”.

## 5. Słowniczek do ujednolicenia
| Pojęcie | Używaj | Nie używaj | Przykłady do zmiany |
|---|---|---|---|
| jeden plik dźwięku z jednego podejścia do mikrofonu | **nagranie** | sesja, podejście, take | `PrompterComponent.h:611` „nowego podejścia”, `MainComponent.h:1879` „nową sesję” |
| cały odcinek wraz z plikami | **odcinek** | projekt, sesja | „Nowy projekt” `ReviewComponent.h:383`, „NOWY PROJEKT” na starcie, „Folder sesji…” `MainComponent.h:442` → „Folder nagrań…” |
| miejsce zapisu | **folder nagrań** | folder sesji, „surowe” bez wyjaśnienia | `MainComponent.h:1163,1787` |
| krok 3 | **Montaż** (w kroku: „Odsłuch i montaż”) | Montaż/Odsłuch zamiennie | `PrompterComponent.h:1664` „ekranie Montaż” OK; kreator ma osobny krok „Odsłuch” = słuchawki → nazwać „Głośność słuchawek” |
| nagranie wersu jeszcze raz w czasie nagrania (R) | **powtórka** („Powtórz wers”) | poprawki po R | `ReviewComponent.h:1371,2293,1424` → „Powtórki” |
| nagranie fragmentu ponownie w montażu | **poprawka** („Nagraj poprawkę”) | Popraw fragment | `ReviewComponent.h:373` „Popraw fragment” → „Nagraj poprawkę” |
| panel ustawień | **Ustawienia** | Mikrofon i sprzęt | blokery pkt 2 |
| wzorzec brzmienia | **moje brzmienie** | wzorzec, barwa | `ReviewComponent.h:1125–1128` |
| plik gotowy | **gotowy odcinek** / plik MP3 | plik końcowy, wersja do publikacji | `ReviewComponent.h:1155,1853` |

## 6. Propozycje tekstów marki
- **Hasło:** „Zielone światło dla Twojego głosu.” (zapas: „Mów. Resztą zajmie się program.”)
- **Opis do strony/instalatora:** „Zielone światło to program do nagrywania podcastów dla ludzi, którzy chcą mówić, a nie kręcić gałkami. Prowadzi Cię krok po kroku: od sprawdzenia mikrofonu, przez nagranie z prompterem, po gotowy plik MP3 o głośności jak w znanych podcastach. Bez wiedzy o dźwięku, bez wtyczek, bez abonamentu.”
- **Komunikat powitalny (pierwsze uruchomienie):** „Cześć! Zanim nagrasz pierwszy odcinek, sprawdźmy w minutę Twój mikrofon i słuchawki. Potem już tylko mówisz – zielona lampka powie Ci, że wszystko gra.” Przyciski: „Sprawdź sprzęt” / „Później”.
- Instalator (`.nsi:48`): obecny tekst „…Twoje nagrania i ustawienia nie są ruszane…” → „…Twoje nagrania i ustawienia zostaną nietknięte – także przy aktualizacji i odinstalowaniu.” (półpauza ze spacjami zamiast pauzy; „ruszane” potoczne).

## 7. Poprawione teksty (stary → nowy)
| # | Plik:linia | Stary | Nowy |
|---|---|---|---|
| 1 | Main.cpp:12, 128 | Zielone swiatlo | Zielone światło |
| 2 | ReviewComponent.h:1128 | …Usunąć wzorzec można w „Mikrofon i sprzęt”. | Kolejne odcinki zabrzmią tak samo. Zmienisz to w Ustawieniach. |
| 3 | ReviewComponent.h:1967 | …Sprawdź „Mikrofon i sprzęt” i nagraj… | …Sprawdź mikrofon w Ustawieniach i nagraj… |
| 4 | PrompterComponent.h:1807 | Wczytaj tekst, zeby czytac z promptera. | Wczytaj tekst, żeby czytać z promptera. |
| 5 | StartComponent.h:148 | Po Starcie przejdziesz do krótkiego przygotowania. | Po kliknięciu Start przygotujemy nagranie. |
| 6 | StartComponent.h:135 | Prompter (ucięte „Promp…”) | Tekst |
| 7 | ReviewComponent.h:2293 | Poprawki po R | Powtórki |
| 8 | ReviewComponent.h:1371 | Najpierw poprawki po R: przy każdej posłuchaj połączenia i zdecyduj — zaznacz do wycięcia albo zostaw. | Najpierw powtórki: posłuchaj każdej i zdecyduj, którą wersję zostawić. |
| 9 | ReviewComponent.h:1424 | Najpierw zdecyduj o poprawkach po R i kliknij „Wytnij zaznaczone”. | Najpierw wybierz wersje powtórek i kliknij „Wytnij zaznaczone”. |
| 10 | ReviewComponent.h:373 | Popraw fragment | Nagraj poprawkę |
| 11 | ReviewComponent.h:383 | Nowy projekt | Nowy odcinek |
| 12 | ReviewComponent.h:384 | Ekran startowy: inna nazwa, inny rodzaj nagrania. Ten projekt zostaje na dysku. | Zacznij kolejny odcinek. Ten zostaje na dysku. |
| 13 | PrompterComponent.h:611 | PAUZA. Czerwona linia wyznacza początek nowego podejścia… | Pauza. Od czerwonej linii nagrasz dalej. Kliknij falę lub wers, aby to zmienić. |
| 14 | PrompterComponent.h:607 | Wznowisz od czerwonej linii. Poprzednia wersja zostanie w folderze „surowe”. Kliknij falę, aby zmienić miejsce. | Nagrasz od czerwonej linii. Poprzednia wersja zostaje zapisana. |
| 15 | MainComponent.h:1879 | Dalszą część nagraj jako nową sesję („Wróć do nagrywania”). | Dalszą część nagraj jako kolejne nagranie („Wróć do nagrywania”). |
| 16 | MainComponent.h:442 | Folder sesji… | Folder nagrań… |
| 17 | ReviewComponent.h:1177 | -19.0 LUFS · szczyt -1.3 dBTP | Głośność −19 LUFS (szczegóły w zaawansowanych) |
| 18 | ReviewComponent.h:1379 | DO POWTÓRZENIA (nie da się naprawić miksem) | Warto nagrać jeszcze raz (miks tego nie naprawi) |
| 19 | WizardComponent.h:404 | …tak głośno, jak sam mówiłeś… | …tak głośno, jak mówisz na co dzień… |
| 20 | ReviewComponent.h:1148/1150 | Nagranie gotowe / Zapisano WAV do archiwum i MP3 do publikacji… | Odcinek gotowy! / Plik MP3 czeka na publikację, WAV zostaje w archiwum. Posłuchaj całości przed wysłaniem. |

## 8. Czego nie sprawdzono
- Instalator NSIS i ikona na Windows (brak Windows w kontenerze); ikona `Resources/ikona.png` nie oceniona na pasku zadań.
- Splash na żywo (tryb podglądu go pomija); sprawdzono tylko kod (`Main.cpp:128`).
- Teksty budowane dynamicznie w `DSP/TakeReview.h` (opisy miksu) – przejrzane wyrywkowo; wyglądają dobrze (zrzut review).
- Tryb `--preview-ui start` z plikiem WAV otwiera ekran Montażu, nie startowy – startowy jest w `kreatywny-witaj.png`.
