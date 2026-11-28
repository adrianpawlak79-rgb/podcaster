#include "../Source/DSP/ReplaceRange.h"

#include <algorithm>
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

    // Szew (QA W5): przenikanie z ciaglym oryginalem, nie ze stala probka.
    {
        constexpr double rate = 48000.0;
        constexpr double pi = 3.14159265358979323846;
        std::vector<float> sine ((size_t) (rate * 2.0));
        for (size_t i = 0; i < sine.size(); ++i)
            sine[i] = (float) (0.5 * std::sin (2.0 * pi * 200.0 * (double) i / rate));
        const size_t a = (size_t) (rate * 0.5), b = (size_t) (rate * 1.0);

        // poprawka = ten sam sinus w tej samej fazie: wynik musi byc oryginalem
        std::vector<float> same (sine.begin() + (long) a, sine.begin() + (long) b), seam;
        double worst = 0.0;
        const bool okSame = tw::ReplaceRange::render (sine, same, rate, 0.5, 1.0, true, seam) && seam.size() == sine.size();
        for (size_t i = 0; okSame && i < seam.size(); ++i) worst = std::max (worst, (double) std::abs (seam[i] - sine[i]));
        check (okSame && worst < 1e-5, "sinus podmieniony tym samym sinusem: brak śladu szwu (różnica < 1e-5)");

        // poprawka = ten sam sinus o polowe cichszy: skok miedzy sasiednimi
        // probkami na obu szwach nie wiekszy niz w samym sinusie
        double sineStep = 0.0;
        for (size_t i = 1; i < sine.size(); ++i) sineStep = std::max (sineStep, (double) std::abs (sine[i] - sine[i - 1]));
        std::vector<float> quiet (same);
        for (auto& v : quiet) v *= 0.5f;
        std::vector<float> q;
        double step = 0.0;
        const bool okQuiet = tw::ReplaceRange::render (sine, quiet, rate, 0.5, 1.0, true, q);
        for (size_t i = a - 50; okQuiet && i < a + 600; ++i) step = std::max (step, (double) std::abs (q[i] - q[i - 1]));
        for (size_t i = b - 600; okQuiet && i < b + 50; ++i) step = std::max (step, (double) std::abs (q[i] - q[i - 1]));
        check (okQuiet && step <= sineStep * 1.05, "cichsza poprawka: brak skoku amplitudy na obu szwach");

        // w srodku przenikania (5 ms) pol oryginalu, pol poprawki
        const size_t mid = a + 239;
        check (okQuiet && std::abs (q[mid] - (sine[mid] * 0.5f + quiet[239] * 0.5f)) < 0.005f,
               "w środku przenikania pół oryginału, pół poprawki");

        // dluzsza poprawka (bez podkladu): koniec przechodzi w ciagly oryginal
        std::vector<float> longer (sine.begin() + (long) a, sine.begin() + (long) (b + 4800)), l;
        for (auto& v : longer) v *= 0.5f;
        double lstep = 0.0;
        const bool okLong = tw::ReplaceRange::render (sine, longer, rate, 0.5, 1.0, false, l);
        const size_t seamAt = a + longer.size();
        for (size_t i = seamAt - 600; okLong && i < seamAt + 50; ++i) lstep = std::max (lstep, (double) std::abs (l[i] - l[i - 1]));
        check (okLong && lstep <= sineStep * 1.6, "dłuższa poprawka: szew bez stuku");
    }

    tw::LineMark before, removed, afterMark, fresh;
    before.sec = 1.0; removed.sec = 3.0; afterMark.sec = 6.0; fresh.sec = 1.0;
    const auto marks = tw::ReplaceRange::remapMarks ({ before, removed, afterMark }, { fresh }, 2.0, 5.0, 2.0);
    check (marks.size() == 3 && marks[0].sec == 1.0 && marks[1].sec == 3.0 && marks[2].sec == 5.0,
           "czasy wersów we fragmencie podmienione, późniejsze przesunięte");

    std::printf (fails ? "BŁĘDY: %d\n" : "Wszystko OK\n", fails);
    return fails ? 1 : 0;
}
