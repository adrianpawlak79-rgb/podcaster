# Nowa szata (13.10.2026)

Adrian: „wszystko zielone jest jednostajne i nudne — zieleń tylko dla zielonego światła”.
Projekt palet i przycisku „Nagraj” zrobił ChatGPT na zlecenie Adriana
(`specyfikacja-kolorow-i-przycisku.txt`, ekrany `paleta-*.png`, przyciski `przycisk-*.png`).

- **A — Studio Copper** (domyślna): akcent miedziany #C47A44.
- **B — Night Console**: akcent niebieski #4F7DFF.
- Paleta C (Warm Graphite) odrzucona: złoty akcent prawie jak żółty „uwaga” lampki.

Zieleń #39CF7B znaczy tylko „dobrze”: lampka, miernik, ✓ przy wersie, „Czytaj”, werdykt,
logo. Kolory pochodne (pasek, wypukłe, krawędź, tekst przeczytany) dobrane do palet tak,
żeby kontrast spełniał progi z `Tests/wyglad-test.cpp`.

W kodzie: `zs::ui::Palette` (`Source/UiRules.h`), przełącznik `--paleta miedz|granat|klasyczna`.
Zrzuty programu: `miedz-prompter.png`, `granat-prompter.png`.
