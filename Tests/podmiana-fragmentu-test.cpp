#include "../Source/DSP/ReplaceRange.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int fails = 0;
static void check (bool ok, const char* name)
{
    std::printf ("  %-5s %s\n", ok ? "OK" : "ZLE", name);
    if (! ok) ++fails;
}

int main()
{
    constexpr double sr = 1000.0;
    std::vector<float> original (10000);
    for (size_t i = 0; i < original.size(); ++i) original[i] = (float) i;
    std::vector<float> replacement (2000, -1.0f), out;

    check (tw::ReplaceRange::render (original, replacement, sr, 2.0, 5.0, false, out),
           "podmiana fragmentu o innej długości działa");
    check (out.size() == original.size() - 3000 + replacement.size(),
           "długość = stara − wycięty fragment + nowy");
    bool after = out.size() == 9000;
    for (size_t i = 0; after && i < 100; ++i)
        after = out[4000 + i] == original[5000 + i];
    check (after, "próbki po szwie są identyczne i przesunięte o różnicę długości");

    std::vector<float> exact (3000, 0.5f), fixed;
    check (tw::ReplaceRange::render (original, exact, sr, 2.0, 5.0, true, fixed)
               && fixed.size() == original.size(),
           "z podkładem podmiana zachowuje długość nagrania");
    check (! tw::ReplaceRange::render (original, replacement, sr, 2.0, 5.0, true, fixed),
           "z podkładem odrzuca fragment o innej długości");

    size_t first = 0, last = 0;
    check (tw::ReplaceRange::sampleBounds (original.size(), sr, 1.0004, 2.0006, first, last)
               && last - first == 1001,
           "granice ułamkowych sekund zaokrąglane osobno dają tę samą liczbę próbek");
    std::vector<float> rounded (1001, 0.25f);
    check (tw::ReplaceRange::render (original, rounded, sr, 1.0004, 2.0006, true, fixed)
               && fixed.size() == original.size(),
           "podkład: ułamkowe granice nie odrzucają poprawki przez różnicę zaokrągleń");

    tw::LineMark before, removed, afterMark, fresh;
    before.sec = 1.0; removed.sec = 3.0; afterMark.sec = 6.0; fresh.sec = 1.0;
    const auto marks = tw::ReplaceRange::remapMarks ({ before, removed, afterMark }, { fresh }, 2.0, 5.0, 2.0);
    check (marks.size() == 3 && marks[0].sec == 1.0 && marks[1].sec == 3.0 && marks[2].sec == 5.0,
           "czasy wersów we fragmencie podmienione, późniejsze przesunięte");

    std::printf (fails ? "BŁĘDY: %d\n" : "Wszystko OK\n", fails);
    return fails ? 1 : 0;
}
