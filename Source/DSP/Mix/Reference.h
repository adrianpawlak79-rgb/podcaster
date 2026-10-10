/*
    Reference.h — nagranie odniesienia zamiast normy.

    ---------------------------------------------------------------------------
    SKAD TO SIE WZIELO

    Czlowiek sluchal siedmiu wersji swojego wiersza i przy kazdej mowil "za
    glosne". Szukalem przyczyny w kompresji, w de-esserze, w barwie. Nie bylo
    jej tam. Byla w tym, ze wszystko renderowalismy do -16 LUFS, bo tak mowi
    norma dla podcastu — a strona porownawcza podglasniala do -16 nawet
    oryginal, zeby ucho nie mylilo glosnosci z jakoscia.

    Potem wlaczyl obok piosenke i powiedzial: moj glos jest glosniejszy.
    Zmierzone:

        jego wlasny miks (FabFilter + soothe)     -20,2 LUFS, cicho przez 16%
        jego wiersz, surowy                       -18,4 LUFS, cicho przez 59%

    Norma -16 LUFS powstala dla mowy sluchanej w samochodzie, gdzie kazde slowo
    ma przebic silnik. Glos sam, bez podkladu, przy tej samej liczbie brzmi
    zupelnie inaczej niz piosenka z perkusja — bo nie ma z czym dzielic miejsca.
    Liczba byla dotrzymana. Cel byl nie z tej bajki.

    ---------------------------------------------------------------------------
    CO Z TEGO WYNIKA

    Program nie powinien mowic czlowiekowi, jak glosne ma byc jego nagranie.
    Powinien umiec zapytac: a jak glosne ma byc? — i przyjac odpowiedz w
    jedynej formie, w ktorej ktos potrafi jej udzielic: "ma brzmiec jak to".

    Wiec bierzemy plik, ktory sie komus podoba, i zdejmujemy z niego liczby:
    glosnosc, szczyt, ile oddycha, ile czasu milczy, jaka ma barwe. To jest
    caly profil. Potem lancuch celuje w niego zamiast w norme.

    Nic z tego nie wymaga, zeby ktokolwiek czegokolwiek sluchal. Ale wymaga
    uczciwosci w drugą stronę: profil zdjety z piosenki z pelnym podkladem NIE
    JEST celem dla samego glosu, i program ma to powiedziec, a nie udawac, ze
    umie zrobic z wiersza Radiohead. Dlatego oprocz liczb notujemy, ile w
    nagraniu jest ciszy — bo to jedna liczba, ktora najlepiej odroznia "glos
    sam" od "glos w miksie".
*/

#pragma once

#include "Loudness.h"
#include "TiltMatch.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mix
{

//==============================================================================
struct Profile
{
    double lufs      = 0.0;    ///< glosnosc scalkowana, BS.1770
    double truePeakDb = 0.0;
    double plr       = 0.0;    ///< szczyt minus glosnosc: ile zostalo zapasu

    double spreadLu  = 0.0;    ///< 90. minus 10. centyl okien 3 s
    double nearPct   = 0.0;    ///< ile czasu w granicach 3 LU od wlasnego szczytu
    double quietPct  = 0.0;    ///< ile czasu 12 LU ponizej niego

    double bandDb[TiltMatch::kBands] {};   ///< barwa, do dopasowania
    bool   valid = false;

    /** Nagranie, w ktorym prawie nie ma ciszy, to prawie na pewno miks, a nie
        glos sam. Cel glosnosci zdjety z takiego pliku bylby dla glosu za
        wysoki — i o tym trzeba powiedziec, a nie po cichu go uzyc. */
    bool looksLikeFullMix() const noexcept { return quietPct < 25.0; }
};

//==============================================================================
class Reference
{
public:
    /** Zdejmuje profil z gotowego nagrania. */
    static Profile measure (const std::vector<float>& x, double sr)
    {
        Profile p;

        if (x.size() < (size_t) (sr * 4.0)) return p;

        p.lufs       = integrated (x, sr);
        p.truePeakDb = truePeak (x, sr);
        p.plr        = p.truePeakDb - p.lufs;

        // ---- rozklad glosnosci w czasie --------------------------------------
        const size_t win = (size_t) (3.0 * sr);
        const size_t hop = (size_t) (0.25 * sr);

        std::vector<double> shortTerm;

        for (size_t i = 0; i + win <= x.size(); i += hop)
        {
            std::vector<float> w (x.begin() + (long) i, x.begin() + (long) (i + win));
            const double l = integrated (w, sr);

            if (l > -70.0) shortTerm.push_back (l);
        }

        if (shortTerm.size() >= 8)
        {
            auto s = shortTerm;
            std::sort (s.begin(), s.end());

            const double top = s[(size_t) (s.size() * 0.95)];

            p.spreadLu = s[(size_t) (s.size() * 0.90)] - s[(size_t) (s.size() * 0.10)];

            int near = 0, quiet = 0;

            for (double v : shortTerm)
            {
                if (v > top - 3.0)  ++near;
                if (v < top - 12.0) ++quiet;
            }

            p.nearPct  = 100.0 * near  / (double) shortTerm.size();
            p.quietPct = 100.0 * quiet / (double) shortTerm.size();
        }

        // ---- barwa ------------------------------------------------------------
        const auto sp = TiltMatch::analyse (x, sr);

        if (sp.valid)
            for (int b = 0; b < TiltMatch::kBands; ++b)
                p.bandDb[b] = bandLevel (sp, sr, b);

        p.valid = true;
        return p;
    }

    //==========================================================================
    /** Zdanie po zdaniu: czym rozni sie miks od odniesienia.

        Kolejnosc nie jest przypadkowa — najpierw to, co slychac najbardziej.
        Roznica glosnosci przykrywa wszystko inne, wiec idzie pierwsza; barwa
        idzie ostatnia, bo przy zlym poziomie nikt jej i tak nie ocenia.
    */
    static std::vector<std::string> compare (const Profile& mix, const Profile& ref)
    {
        std::vector<std::string> out;

        if (! mix.valid || ! ref.valid)
        {
            out.push_back ("Za krotkie nagranie, zeby cokolwiek z niego zdjac.");
            return out;
        }

        char b[256];

        // ---- glosnosc ---------------------------------------------------------
        const double dL = mix.lufs - ref.lufs;

        if (std::fabs (dL) < 0.7)
            out.push_back ("Glosnosc taka sama jak w odniesieniu.");
        else
        {
            std::snprintf (b, sizeof (b), "Glosnosc %s o %.1f dB (%.1f zamiast %.1f LUFS).",
                           dL > 0 ? "wieksza" : "mniejsza", std::fabs (dL), mix.lufs, ref.lufs);
            out.push_back (b);
        }

        // ---- ile oddycha ------------------------------------------------------
        const double dS = mix.spreadLu - ref.spreadLu;

        if (std::fabs (dS) >= 3.0)
        {
            std::snprintf (b, sizeof (b),
                           "%s: rozrzut %.0f LU wobec %.0f w odniesieniu.",
                           dS < 0 ? "Mniej oddycha" : "Wiecej oddycha",
                           mix.spreadLu, ref.spreadLu);
            out.push_back (b);
        }

        // ---- ile czasu jest cicho ---------------------------------------------
        const double dQ = mix.quietPct - ref.quietPct;

        if (std::fabs (dQ) >= 8.0)
        {
            std::snprintf (b, sizeof (b),
                           "Cisza zajmuje %.0f%% czasu, w odniesieniu %.0f%%.",
                           mix.quietPct, ref.quietPct);
            out.push_back (b);
        }

        // ---- zapas na szczyty -------------------------------------------------
        if (mix.plr < ref.plr - 2.0)
        {
            std::snprintf (b, sizeof (b),
                           "Mniej zapasu na szczyty: %.1f dB wobec %.1f.",
                           mix.plr, ref.plr);
            out.push_back (b);
        }

        // ---- barwa -------------------------------------------------------------
        double worst = 0.0;
        int worstBand = -1;

        const double anchorMix = average (mix.bandDb);
        const double anchorRef = average (ref.bandDb);

        for (int i = 0; i < TiltMatch::kBands; ++i)
        {
            const double d = (mix.bandDb[i] - anchorMix) - (ref.bandDb[i] - anchorRef);

            if (std::fabs (d) > std::fabs (worst)) { worst = d; worstBand = i; }
        }

        if (worstBand >= 0 && std::fabs (worst) >= 2.0)
        {
            std::snprintf (b, sizeof (b), "Barwa: %s o %.1f dB w okolicy %.0f Hz.",
                           worst > 0 ? "wiecej" : "mniej",
                           std::fabs (worst), TiltMatch::centres[worstBand]);
            out.push_back (b);
        }

        if (out.empty())
            out.push_back ("Nic, co dalo by sie zmierzyc, nie rozni tych dwoch nagran.");

        return out;
    }

    //==========================================================================
    /** Co wolno wziac z odniesienia, a czego nie — i dlaczego.

        To jest miejsce, w ktorym program moglby po cichu zrobic krzywde. Profil
        zdjety z gotowego miksu zawiera nie tylko glosnosc, ale tez gestosc: miks
        z perkusja i gitarami siedzi przy swoim szczycie przez 41% czasu, bo
        wypelniaja go instrumenty. Sam glos moze osiagnac te sama liczbe tylko
        jednym sposobem — przez sciskanie, ktore zabije mu oddech.

        Zmierzone na wierszu: cel zdjety z gotowego miksu scisnal rozrzut z 37,7
        LU do 14,8 i cisze z 59% do 16%. Profil zostal dotrzymany co do liczby.
        Nagranie zostalo zniszczone.

        Wiec program bierze z odniesienia glosnosc zawsze, barwe prawie zawsze, a
        dynamike tylko wtedy, gdy oba nagrania sa tego samego rodzaju. I mowi,
        czego nie wzial.
    */
    struct Plan
    {
        double targetLufs   = -16.0;
        bool   matchDynamics = true;
        bool   matchTone     = true;
        std::vector<std::string> notes;
    };

    //==========================================================================
    /** Najwyzsza glosnosc, przy ktorej najglosniejszy moment zostaje nietkniety.

        Czlowiek powiedzial: "norma podcastowa jest ok dopoki nie krzycze w
        refrenie w 33 sekundzie". To bylo pytanie, ktore dalo sie policzyc.

        Zapas miedzy szczytem a glosnoscia (PLR) mowi, ile miejsca potrzebuje
        najglosniejszy moment nagrania. Sufit mowi, ile miejsca jest. Roznica to
        glosnosc, powyzej ktorej limiter MUSI zaczac ten moment sciagac — bo
        arytmetyka nie zostawia innego wyjscia.

        Na jego wierszu: zapas 17,4 dB, sufit -1,0 dBTP, czyli -18,4 LUFS. Pomiar
        sekunda po sekundzie pokazal, ze scinanie krzyku przestaje rosnac ponizej
        -18. Rachunek i pomiar zgodzily sie co do polowy decybela, a to znaczy, ze
        rozumiemy mechanizm, a nie tylko go opisujemy.

        Norma -16 LUFS nie jest przez to zla. Jest odpowiedzia na inne pytanie:
        jak zrobic, zeby kazde slowo przebilo silnik samochodu. Przy materiale, w
        ktorym najglosniejszy moment JEST trescia, te dwa cele sie wykluczaja i
        program ma o tym powiedziec, a nie po cichu wybrac za czlowieka.
    */
    static double loudestUntouched (const Profile& p, double ceilingDbTp = -1.0)
    {
        if (! p.valid) return -16.0;

        return std::clamp (ceilingDbTp - p.plr, -30.0, -9.0);
    }

    /** Czy zadany cel wymaga ruszenia najglosniejszego momentu, i o ile. */
    static double squashAtTarget (const Profile& p, double targetLufs,
                                  double ceilingDbTp = -1.0)
    {
        return std::max (0.0, targetLufs - loudestUntouched (p, ceilingDbTp));
    }

    static Plan plan (const Profile& src, const Profile& ref, bool sourceIsVoiceAlone)
    {
        Plan p;

        if (! ref.valid)
        {
            p.notes.push_back ("Nie udalo sie zmierzyc odniesienia — zostaje norma.");
            p.matchDynamics = p.matchTone = false;
            return p;
        }

        const bool refIsMix = ref.looksLikeFullMix();
        const bool mismatch = sourceIsVoiceAlone && refIsMix;

        p.targetLufs = std::clamp (ref.lufs - (mismatch ? 2.0 : 0.0), -30.0, -9.0);

        char b[400];

        if (mismatch)
        {
            p.matchDynamics = false;
            p.matchTone     = false;

            std::snprintf (b, sizeof (b),
                "To odniesienie to gotowy miks: cisza zajmuje w nim %.0f%% czasu, "
                "a w Twoim nagraniu %.0f%%. Biore z niego glosnosc, obnizona o 2 dB, "
                "bo sam glos przy tej samej liczbie brzmi mocniej niz glos w miksie.",
                ref.quietPct, src.quietPct);
            p.notes.push_back (b);

            p.notes.push_back (
                "NIE biore z niego gestosci ani barwy. Zeby glos sam siedzial przy "
                "szczycie tak czesto jak miks, trzeba by go scisnac tak, ze przestalby "
                "oddychac — a barwa miksu to barwa wszystkich instrumentow razem, "
                "nie glosu.");
        }
        else
        {
            std::snprintf (b, sizeof (b), "Cel glosnosci z odniesienia: %.1f LUFS.", p.targetLufs);
            p.notes.push_back (b);

            if (src.spreadLu > ref.spreadLu + 8.0)
            {
                std::snprintf (b, sizeof (b),
                    "Twoje nagranie oddycha znacznie szerzej (%.0f LU wobec %.0f). "
                    "Zeby zejsc do odniesienia, kompresja musi wejsc mocno — jesli "
                    "dynamika jest tu trescia, lepiej zostawic ja w spokoju.",
                    src.spreadLu, ref.spreadLu);
                p.notes.push_back (b);
            }
        }

        return p;
    }

private:
    static double average (const double v[TiltMatch::kBands])
    {
        double s = 0.0;
        for (int i = 0; i < TiltMatch::kBands; ++i) s += v[i];
        return s / (double) TiltMatch::kBands;
    }

    static double bandLevel (const TiltMatch::Spectrum& sp, double sr, int b)
    {
        const double c = TiltMatch::centres[b];
        const double lo = c / std::pow (2.0, 1.0 / 3.0);
        const double hi = c * std::pow (2.0, 1.0 / 3.0);

        double sum = 0.0;
        int n = 0;

        for (size_t i = 0; i < sp.bin.size(); ++i)
        {
            const double f = (double) i * sp.hzPerBin;
            if (f >= lo && f < hi) { sum += sp.bin[i]; ++n; }
        }

        (void) sr;
        return n > 0 ? 10.0 * std::log10 (sum / (double) n + 1.0e-30) : -120.0;
    }

    static double integrated (const std::vector<float>& x, double sr)
    {
        LoudnessMeter m;
        m.prepare (sr, 1);
        const float* p[1] = { x.data() };
        m.process (p, 1, (int) x.size());
        return m.integrated();
    }

    static double truePeak (const std::vector<float>& x, double sr)
    {
        TruePeak t;
        t.prepare (sr, 1);
        const float* p[1] = { x.data() };
        t.process (p, 1, (int) x.size());
        t.finish();
        return t.dbTP();
    }
};

} // namespace mix
