/*
    podklad-montaz-test.cpp — ciecia przy podkladzie nie przesuwaja glosu
    wzgledem muzyki (Source/DSP/BackingEdit.h).

    Najwazniejsze: probka zaraz po wycinku lezy w pliku dokladnie na
    EditedView::editedTime — tam, gdzie podczas nagrania wystartowal rozbieg
    muzyki. Do porownania: Punch::render przesuwa ja o 20 ms na kazdym cieciu.
*/

#include "../Source/DSP/BackingEdit.h"
#include "../Source/DSP/EditedView.h"
#include "../Source/DSP/Punch.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-62s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

int main()
{
    const double sr = 48000.0;
    // znacznik: kazda probka = jej numer (latwo sprawdzic, skad przyszla)
    std::vector<float> in (sr * 20);
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float) i;

    std::printf ("1. Suma zbiorow odcinkow\n");
    {
        const auto c = tw::BackingEdit::exactCuts ({ { 5.0, 7.0 }, { 2.0, 3.0 }, { 6.5, 8.0 }, { 9.0, 9.0 }, { 8.0, 8.5 } });
        check (c.size() == 2, "zachodzace i stykajace sie sklejone, puste pominiete", std::to_string (c.size()));
        check (c.size() == 2 && c[0].first == 2.0 && c[1].first == 5.0 && c[1].second == 8.5, "granice 2-3 i 5-8,5");
    }

    std::printf ("2. Glos po cieciu tam, gdzie gral rozbieg muzyki\n");
    {
        const tw::EditedView::Spans spans = { { 3.0, 5.0 }, { 8.0, 11.5 }, { 10.0, 12.0 } };
        const auto out = tw::BackingEdit::renderExact (in, sr, spans);
        const auto removed = 2.0 + 4.0;   // 3-5 i suma 8-12
        check (out.size() == in.size() - (size_t) (removed * sr), "dlugosc = surowe - suma wycinkow",
               std::to_string ((long) in.size() - (long) out.size()));

        // za przenikaniem (10 ms) probki musza byc dokladnie z miejsca po wycinku
        bool exact = true;
        for (double resume : { 5.0, 12.0 })
        {
            const double at = tw::EditedView::editedTime (resume, spans);
            const size_t j = (size_t) std::llround ((at + 0.02) * sr);
            const size_t want = (size_t) std::llround ((resume + 0.02) * sr);
            if (j >= out.size() || std::abs (out[j] - (float) want) > 0.5f) exact = false;
        }
        check (exact, "wznowienie po 5 s i po 12 s co do probki na editedTime");

        bool before = true;
        for (size_t i = 0; i < (size_t) (3.0 * sr); i += 997) if (out[i] != (float) i) before = false;
        check (before, "przed pierwszym cieciem bez zmian");

        // Punch::render na tych samych odcinkach — dla porownania
        tw::Punch punch;
        std::vector<tw::Removal> plan = { { 3.0, 5.0 }, { 8.0, 12.0 } };
        const auto old = punch.render (in, sr, plan);
        const double drift = ((double) out.size() - (double) old.size()) / sr * 1000.0;
        check (drift > 30.0, "Punch::render zjada wiecej (dlatego osobna droga)", std::to_string (drift) + " ms");
    }

    std::printf ("3. Wyciszanie w montazu: dlugosc ta sama\n");
    {
        std::vector<float> a (sr * 4, 1.0f);
        const bool ok = tw::BackingEdit::muteRange (a, sr, 1.0, 2.0);
        check (ok && a.size() == (size_t) (sr * 4), "dlugosc bez zmian");
        check (std::abs (a[(size_t) (1.5 * sr)] - 0.001f) < 1e-6f, "srodek wyciszony o 60 dB (nie cyfrowa cisza)");
        check (a[(size_t) (0.99 * sr)] == 1.0f && a[(size_t) (2.01 * sr)] == 1.0f, "poza odcinkiem bez zmian");
        float jump = 0.0f;
        for (size_t i = 1; i < a.size(); ++i) jump = std::max (jump, std::abs (a[i] - a[i - 1]));
        check (jump < 0.01f, "brzegi bez skoku (wygaszenie 10 ms)", std::to_string (jump));
        check (! tw::BackingEdit::muteRange (a, sr, 2.0, 2.0), "pusty odcinek odrzucony");
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
