# Ujednolicenie nazw — stary tekst → nowy

Słowniczek: projekt = całość (nazwa, tekst, podkład); nagranie = jeden zapis głosu; „Nagraj od nowa” = nowe nagranie tego samego projektu; poprawka = nagranie fragmentu po nagraniu („Popraw fragment” → „Nagraj poprawkę” → „Wstaw poprawkę”); „Nagraj od linii” i „Powtórz wers” = czynności w trakcie nagrywania; „Folder nagrań”; folder „surowe” = „surowe (oryginały)”. Numery wierszy dotyczą pliku po zmianie (liczba wierszy się nie zmienia).

| Stary tekst | Nowy tekst | Plik:wiersz |
|---|---|---|
| `— nagraj te fragmenty jeszcze raz, trochę ciszej."` | `— nagraj poprawki tych fragmentów, trochę ciszej."` | Source/DSP/Mix/Decide.h:527 |
| `"Jeden fragment trzeba nagrać jeszcze raz: %s. Tego miks nie cofa."` | `"Jeden fragment wymaga poprawki: %s. Tego miks nie naprawi."` | Source/DSP/TakeReview.h:526 |
| `"Fragmentów do powtórzenia: %d. Wykryłem` | `"Fragmentów do poprawki: %d. Wykryłem` | Source/DSP/TakeReview.h:530 |
| `"nie trzeba go nagrywać od nowa."` | `"nie wymaga poprawki."` | Source/DSP/TakeReview.h:553 |
| `"nie trzeba ich nagrywać od nowa."` | `"nie wymagają poprawki."` | Source/DSP/TakeReview.h:557 |
| `wymagających powtórzenia."` | `wymagających poprawki."` | Source/DSP/TakeReview.h:562 |
| `Sprawdź mikrofon w Ustawieniach i nagraj jeszcze raz."` | `Sprawdź mikrofon w Ustawieniach i nagraj od nowa."` | Source/DSP/TakeReview.h:740 |
| `PL ("Folder sesji…")` | `PL ("Folder nagrań…")` | Source/MainComponent.h:472 |
| `PL ("--- wyciszone w tej sesji: ")` | `PL ("--- wyciszone w tym nagraniu: ")` | Source/MainComponent.h:492 |
| `PL ("Nagraj, a potem odsłuchaj podejście")` | `PL ("Nagraj, a potem odsłuchaj nagranie")` | Source/MainComponent.h:701 |
| `PL (" min do końca sesji")` | `PL (" min do końca nagrania")` | Source/MainComponent.h:1046 |
| `wzorzec brzmienia i folder sesji.")` | `wzorzec brzmienia i folder nagrań.")` | Source/MainComponent.h:1229 |
| `PL ("Czy podmienić fragment?")` | `PL ("Czy wstawić poprawkę?")` | Source/MainComponent.h:1646 |
| `PL ("Poprawka jest gotowa. Podmienić nią zaznaczone miejsce? Oryginał zostanie zachowany w folderze surowe.")` | `PL ("Poprawka jest gotowa. Wstawić ją w zaznaczone miejsce? Oryginał zostanie zachowany w folderze surowe (oryginały).")` | Source/MainComponent.h:1647 |
| `PL ("Podmień fragment")` | `PL ("Wstaw poprawkę")` | Source/MainComponent.h:1648 |
| `PL ("--- podmieniono fragment; poprzednia wersja w folderze surowe")` | `PL ("--- wstawiono poprawkę; poprzednia wersja w folderze surowe (oryginały)")` | Source/MainComponent.h:1674 |
| `PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe.")` | `PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe (oryginały).")` | Source/MainComponent.h:1723 |
| `PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe.")` | `PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe (oryginały).")` | Source/MainComponent.h:1726 |
| `PL (" s; pełne nagranie w folderze „surowe”")` | `PL (" s; pełne nagranie w folderze „surowe” (oryginały)")` | Source/MainComponent.h:1759 |
| `PL ("--- NIE wycięłem pomyłki ")` | `PL ("--- Nie wycięłem pomyłki ")` | Source/MainComponent.h:1774 |
| `PL ("Gdzie zapisywać sesje?")` | `PL ("Gdzie zapisywać nagrania?")` | Source/MainComponent.h:1904 |
| `append ("folder sesji: "` | `append ("folder nagrań: "` | Source/MainComponent.h:1915 |
| `PL ("--- nagrywanie zatrzymane: sesja ma 60 minut")` | `PL ("--- nagrywanie zatrzymane: nagranie ma 60 minut")` | Source/MainComponent.h:1993 |
| `PL ("Sesja ma 60 minut")` | `PL ("Nagranie ma 60 minut")` | Source/MainComponent.h:1994 |
| `PL ("Tyle program sprawdził w jednej sesji, więc` | `PL ("Tyle program sprawdził w jednym nagraniu, więc` | Source/MainComponent.h:1995 |
| `PL ("Dalszą część nagraj jako nowe podejście („Nagraj nowe podejście” w montażu).")` | `PL ("Dalszą część nagraj jako nowe nagranie („Nagraj od nowa” w montażu).")` | Source/MainComponent.h:1997 |
| `tRecord.setTooltip (PL ("Nagraj zaznaczony fragment jeszcze raz"));` | `tRecord.setTooltip (PL ("Nagraj poprawkę zaznaczonego fragmentu"));` | Source/PrompterComponent.h:165 |
| `PL ("Wróć na początek wersu i przeczytaj go jeszcze raz (R)")` | `PL ("Wróć na początek wersu i przeczytaj go jeszcze raz — nagranie trwa dalej (R)")` | Source/PrompterComponent.h:592 |
| `replacementMode ? PL ("Nagraj zaznaczony fragment jeszcze raz")` | `replacementMode ? PL ("Nagraj poprawkę zaznaczonego fragmentu")` | Source/PrompterComponent.h:609 |
| `Poprzednia wersja zostanie w folderze „surowe”. Kliknij falę, aby zmienić miejsce.")` | `Poprzednia wersja zostanie w folderze „surowe” (oryginały). Kliknij falę, aby zmienić miejsce.")` | Source/PrompterComponent.h:610 |
| `Czerwona linia wyznacza początek nowego podejścia. Kliknij falę lub wers, aby wybrać inne miejsce; poprzednia wersja zostanie w folderze „surowe”.")` | `Czerwona linia wyznacza miejsce, od którego wznowisz nagranie. Kliknij falę lub wers, aby wybrać inne miejsce; poprzednia wersja zostanie w folderze „surowe” (oryginały).")` | Source/PrompterComponent.h:614 |
| `PL ("stąd nagrasz od nowa")` | `PL ("stąd wznowisz")` | Source/PrompterComponent.h:1036 |
| `tak daleko nie nagrasz od nowa; popraw go w Montażu"` | `tak daleko nie wznowisz; nagraj poprawkę w Montażu"` | Source/PrompterComponent.h:1111 |
| `PL ("PAUZA  ·  za daleko wstecz — popraw w Montażu")` | `PL ("PAUZA  ·  za daleko wstecz — nagraj poprawkę w Montażu")` | Source/PrompterComponent.h:1112 |
| `od którego chcesz nagrać od nowa  ·  kółko` | `od którego chcesz wznowić  ·  kółko` | Source/PrompterComponent.h:1115 |
| `skąd chcesz nagrać od nowa, i Nagraj od linii.` | `skąd chcesz wznowić, i Nagraj od linii.` | Source/PrompterComponent.h:1830 |
| `UWAGA: podczas zapisu utracono dane. Nagraj materiał ponownie.` | `UWAGA: podczas zapisu utracono dane. Nagraj od nowa.` | Source/Recorder.h:96 |
| `PL("Nagraj ten fragment od nowa. Zaznacz go na fali.")` | `PL("Nagraj poprawkę wybranego fragmentu. Zaznacz go na fali.")` | Source/ReviewComponent.h:345 |
| `PL("Nagraj nowe podejście")` | `PL("Nagraj od nowa")` | Source/ReviewComponent.h:399 |
| `PL("Nagraj ten sam tekst jeszcze raz od początku. To nagranie zostaje na dysku.")` | `PL("Nowe nagranie tego samego projektu (ten sam tekst i podkład). Obecne nagranie zostaje na dysku.")` | Source/ReviewComponent.h:400 |
| `PL("Najpierw poprawki po R: przy każdej` | `PL("Najpierw powtórki po R: przy każdej` | Source/ReviewComponent.h:1485 |
| `PL("DO POWTÓRZENIA (nie da się naprawić miksem)\n")` | `PL("Do poprawki (miksem tego nie naprawisz)\n")` | Source/ReviewComponent.h:1493 |
| `a po nagraniu zdecyduj o podmianie.")` | `a po nagraniu zdecyduj, czy wstawić poprawkę.")` | Source/ReviewComponent.h:1508 |
| `PL("Najpierw zdecyduj o poprawkach po R i kliknij` | `PL("Najpierw zdecyduj o powtórkach po R i kliknij` | Source/ReviewComponent.h:1538 |
| `PL("Surowe nagranie, jeszcze bez miksu. Najpierw zdecyduj o poprawkach po R.")` | `PL("Surowe nagranie, jeszcze bez miksu. Najpierw zdecyduj o powtórkach po R.")` | Source/ReviewComponent.h:1756 |
| `leży w folderze „surowe” obok nagrania.` | `leży w folderze „surowe” (oryginały) obok nagrania.` | Source/ReviewComponent.h:1763 |
| `PL ("słychać: PO miksie")` | `PL ("słychać: po miksie")` | Source/ReviewComponent.h:1899 |
| `PL ("słychać: PRZED miksem")` | `PL ("słychać: przed miksem")` | Source/ReviewComponent.h:1900 |
| `Podczas zapisu utracono dane. Nagraj materiał ponownie.` | `Podczas zapisu utracono dane. Nagraj od nowa.` | Source/ReviewComponent.h:2089 |
| `Sprawdź mikrofon w Ustawieniach i nagraj jeszcze raz.")` | `Sprawdź mikrofon w Ustawieniach i nagraj od nowa.")` | Source/ReviewComponent.h:2094 |
| `PL ("powtórz ")` | `PL ("popraw ")` | Source/ReviewComponent.h:2299 |
| `PL("Stosuję poprawki…")` | `PL("Stosuję cięcia…")` | Source/ReviewComponent.h:2395 |
| `PL("Nie udało się zastosować poprawek: ")` | `PL("Nie udało się zastosować cięć: ")` | Source/ReviewComponent.h:2410 |
| `PL("Nie udało się zastosować poprawek. Sprawdź` | `PL("Nie udało się zastosować cięć. Sprawdź` | Source/ReviewComponent.h:2413 |
| `PL("Nie udało się zachować pełnego nagrania i listy w folderze surowe.")` | `PL("Nie udało się zachować pełnego nagrania i listy w folderze surowe (oryginały).")` | Source/ReviewComponent.h:2416 |
| `Pełne nagranie jest w folderze surowe.")` | `Pełne nagranie jest w folderze surowe (oryginały).")` | Source/ReviewComponent.h:2419 |
| `PL("Poprawki po R")` | `PL("Powtórki po R")` | Source/ReviewComponent.h:2420 |
| `Pełne nagranie jest w folderze surowe.")` | `Pełne nagranie jest w folderze surowe (oryginały).")` | Source/ReviewComponent.h:2426 |
| `PL("Poprawki po R")` | `PL("Powtórki po R")` | Source/ReviewComponent.h:2435 |
| `line.find ("jeszcze raz") / "prosi o ponowne nagranie"` | `line.find ("nagraj poprawki") / "prosi o nagranie poprawek"` | Tests/przester-test.cpp:62 |

Razem: 60 zmian tekstów (w tym jedna w teście).
