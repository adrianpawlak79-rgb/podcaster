/*
    Setup.h — kreator sprawdzenia sprzetu. To, co program robi ZANIM padnie
    pierwsze slowo.

    Po co to jest, w kolejnosci waznosci:

    1. Bo tu daje sie naprawic rzeczy, ktorych po nagraniu nie da sie naprawic
       NIGDY. Przester, przydzwiek sieciowy i szum wentylatora pod mowa to nie sa
       problemy miksu — to sa problemy dziesieciu sekund przed nagraniem.

    2. Bo polowa maili od uzytkownikow programow dzwiekowych brzmi "u mnie nie
       ma dzwieku". Kreator odpowiada na to zanim ktokolwiek zdazy napisac.

    3. Bo to jest jedyna czesc tego programu, ktora dziala samodzielnie i daje
       sie pokazac komus w minute.

    ZASADA: kazdy krok konczy sie LICZBA, nie wrazeniem. "Wyglada dobrze" nie
    jest wynikiem. Dlatego kazdy krok ma warunek zaliczenia wyrazony w
    decybelach albo w hercach, i mowi, co konkretnie zrobic, gdy go nie spelnia.

    Moduł nie dotyka dzwieku i nie zna interfejsu uzytkownika. Dostaje probki i
    czas, zwraca stan i zdanie po polsku.
*/

#pragma once
#include <atomic>

#include "Mix/Biquad.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tw
{

//==============================================================================
enum class SetupStep
{
    Device,     ///< czy to na pewno TEN mikrofon
    Room,       ///< cisza w pokoju: szum, przydzwiek, poprawki systemu
    Level,      ///< wzmocnienie i odleglosc
    Plosive,    ///< czy "p" uderza w membrane
    Bleed,      ///< czy slychac sluchawki w mikrofonie
    Monitor,    ///< jak glosno gra odsluch — mierzone Twoim wlasnym glosem
    Done
};

struct SetupFinding
{
    bool        passed = false;
    std::string headline;    ///< jedno zdanie, duzymi literami na ekranie
    std::string detail;      ///< co zrobic, gdy nie zaliczone
    std::string measured;    ///< liczba, ktora o tym zdecydowala
};

struct SetupReport
{
    double noiseFloorDb   = -140.0;
    double humHz          = 0.0;
    double humExcessDb    = 0.0;
    double speechPeakDb   = -140.0;
    double speechRmsDb    = -140.0;
    double gainAdviceDb   = 0.0;
    double proximityDb    = 0.0;   ///< nadmiar dolu = za blisko
    double plosiveExcessDb = 0.0;
    double bleedPercent   = 0.0;

    /* ---- kalibracja odsluchu -------------------------------------------------

       Bez tego wszystkie pozostale liczby sa prawdziwe i bezuzyteczne.

       Czlowiek, ktory zna LUFS, ma wlasny miks na dysku i wlasnie czytal tabele
       pomiarow, przez cztery rundy odsluchu mowil "za glosne" — i na koncu
       okazalo sie, ze mial przykrecony YouTube i rozkrecona karte. Sam to
       zlapal i sam podsumowal: "widzisz, ja mam wiedze i i tak sie lapie".

       Zaden pomiar w pliku nie powie, jak glosno gra karta. Potrzebny jest punkt
       zerowy, i to taki, ktorego nikt nie musi brac na wiare. */
    double monitorVsVoiceDb = 0.0;   ///< odsluch minus wlasny glos, w dB
    bool   monitorMeasured  = false; ///< false = sluchawki, mikrofon nic nie slyszal

    bool   suppressionSuspected = false;   ///< systemowa redukcja szumu
    bool   agcSuspected         = false;   ///< automatyczne wzmocnienie

    bool   ready = false;
};

//==============================================================================
class SetupWizard
{
public:
    struct Limits
    {
        double quietRoomDb   = -58.0;  ///< tlo ponizej tego to dobry pokoj
        double okRoomDb      = -45.0;  ///< ...a ponizej tego da sie pracowac
        double humMarginDb   = 10.0;   ///< o tyle 50 Hz musi wystawac, zeby to byl przydzwiek
        double peakLowDb     = -18.0;  ///< szczyty mowy ponizej = za cicho
        double peakHighDb    = -6.0;   ///< ...powyzej = za glosno
        /* PROWIZORKA — do sprawdzenia na prawdziwym mikrofonie.

           Efekt zblizeniowy to jedyna rzecz w tym kreatorze, ktorej nie da sie
           uczciwie wyskalowac bez mikrofonu: syntezator mowy go nie ma, a moje
           udawanie go polka niskotonowa jest przyblizeniem przyblizenia. Na
           mowie z espeaka zwykla odleglosc daje okolo -8 dB, a udawane
           zblizenie okolo -3 dB.

           Zeby to ustawic naprawde, potrzebne sa dwa zdania z tego samego
           mikrofonu: jedno z odleglosci dloni, drugie z pieciu centymetrow.
           Do tego czasu prog stoi miedzy zmierzonymi wartosciami i moze
           przepuszczac zbyt bliskie ustawienie. */
        double proximityDb   = -5.0;   ///< nadmiar dolu ponad tym = za blisko
        /* Zmierzone, nie wybrane: czysta mowa bez plozyw daje 10 dB nadmiaru,
           to samo nagranie z doklejonym uderzeniem 65 Hz — 32 dB. Prog stoi
           miedzy nimi, blizej dolnej granicy, bo lepiej raz zapytac o popfiltr
           niz przepuscic uderzenie.

           Do sprawdzenia na prawdziwym glosie: syntezator ma slabe zwarcia,
           wiec czlowiek bez plozyw moze mierzyc sie wyzej niz te 10 dB. */
        double plosiveDb     = 18.0;   ///< uderzenie ponad tym = trzeba popfiltr
        double bleedPercent  = 4.0;    ///< przesluch ponad tym bedzie slychac
    };

    Limits limits;

    //--------------------------------------------------------------------------
    /** Ile sekund trwa dany krok i co powiedziec uzytkownikowi. */
    static double seconds (SetupStep s)
    {
        switch (s)
        {
            case SetupStep::Device:  return 3.0;
            case SetupStep::Room:    return 6.0;
            case SetupStep::Level:   return 6.0;
            case SetupStep::Plosive: return 4.0;
            case SetupStep::Bleed:   return 3.0;
            case SetupStep::Monitor: return 5.0;
            default:                 return 0.0;
        }
    }

    static const char* instruction (SetupStep s)
    {
        switch (s)
        {
            case SetupStep::Device:  return "Powiedz cokolwiek — sprawdzam, czy słyszę TEN mikrofon.";
            case SetupStep::Room:    return "Teraz cisza. Nie ruszaj się przez sześć sekund.";
            case SetupStep::Level:   return "Powiedz dwa zdania normalnym głosem, tak jak będziesz mówić.";
            case SetupStep::Plosive: return "Powiedz: „Piotr popija pyszną kawę”.";
            case SetupStep::Bleed:   return "Załóż słuchawki i milcz — sprawdzam, czy z nich nie ucieka.";
            case SetupStep::Monitor: return "Teraz usłyszysz siebie. Ustaw głośność tak, "
                                            "żeby brzmiało jak Ty przed chwilą.";
            default:                 return "";
        }
    }

    //--------------------------------------------------------------------------
    void begin (double sampleRate)
    {
        sr = sampleRate;
        current = SetupStep::Device;

        // Rezerwacja raz, na najdluzszy krok. feed() wola push_back z watku
        // dzwieku, a alokacja w tym watku to przerwa w nagrywaniu — clear()
        // zostawia pojemnosc, wiec wystarczy zrobic to tutaj.
        buffer.reserve ((size_t) (6.5 * sr));
        reference.reserve ((size_t) (3.5 * sr));

        buffer.clear();
        captured.store(0);
        report = {};
        findings.clear();
    }

    SetupStep step() const noexcept { return current; }

    /** Ile z biezacego kroku juz zebrane, 0..1. */
    float progress() const
    {
        const double want = seconds (current) * sr;
        return want > 0.0 ? (float) std::min (1.0, captured.load() / want) : 1.0f;
    }

    bool stepComplete() const { return progress() >= 1.0f; }

    /** Dokladaj probki, az stepComplete(). Wolane z watku dzwieku. */
    std::atomic<size_t> captured{0};

    void feed (const float* in, int numSamples)
    {
        if (current == SetupStep::Done) return;

        const size_t want = (size_t) (seconds (current) * sr);

        for (int i = 0; i < numSamples && buffer.size() < want; ++i)
            buffer.push_back (in[i]);
        captured.store(buffer.size());
    }

    /** Dla kroku z przesluchem: co bylo odtwarzane, do porownania. */
    void feedReference (const float* ref, int numSamples)
    {
        const size_t want = (size_t) (seconds (SetupStep::Bleed) * sr);

        for (int i = 0; i < numSamples && reference.size() < want; ++i)
            reference.push_back (ref[i]);
    }

    /** Ocenia zebrany krok. Wolane z watku interfejsu, gdy stepComplete(). */
    SetupFinding evaluate()
    {
        SetupFinding f;

        switch (current)
        {
            case SetupStep::Device:  f = judgeDevice();  break;
            case SetupStep::Room:    f = judgeRoom();    break;
            case SetupStep::Level:   f = judgeLevel();   break;
            case SetupStep::Plosive: f = judgePlosive(); break;
            case SetupStep::Bleed:   f = judgeBleed();   break;
            case SetupStep::Monitor: f = judgeMonitor(); break;
            default: break;
        }

        findings.push_back (f);
        return f;
    }

    /** Przejscie dalej. Kreator NIE blokuje na niezaliczonym kroku — czlowiek
        ma prawo powiedziec "wiem, jedziemy dalej". Blokowanie na ostrzezeniu
        jest najszybsza droga do tego, zeby ktos wylaczyl caly kreator. */
    void advance()
    {
        // Glos z kroku "Poziom" zostaje — posluzy za wzorzec przy kalibracji.
        if (current == SetupStep::Level)
            ownVoice = buffer;

        buffer.clear();
        captured.store(0);
        reference.clear();

        switch (current)
        {
            case SetupStep::Device:  current = SetupStep::Room;    break;
            case SetupStep::Room:    current = SetupStep::Level;   break;
            case SetupStep::Level:   current = SetupStep::Plosive; break;
            case SetupStep::Plosive: current = SetupStep::Bleed;   break;
            case SetupStep::Bleed:   current = SetupStep::Monitor; break;
            case SetupStep::Monitor: current = SetupStep::Done;    break;
            default: break;
        }

        if (current == SetupStep::Done)
        {
            report.ready = true;

            for (const auto& f : findings)
                if (! f.passed)
                    report.ready = false;
        }
    }

    void repeat() { buffer.clear(); reference.clear(); }

    /** Co odtwarzac w kroku kalibracji: wlasny glos czlowieka, wyrownany do
        poziomu, przy ktorym program bedzie mu pozniej podawal liczby.

        Pusty wektor znaczy: krok "Poziom" zostal pominiety, nie ma czym
        kalibrowac.

        @param targetRms  wartosc skuteczna, do ktorej wyrownac (liniowo)
    */
    std::vector<float> calibrationAudio (float targetRms = 0.05f) const
    {
        if (ownVoice.size() < (size_t) (sr * 0.5)) return {};

        double sum = 0.0;
        int n = 0;

        // Tylko fragmenty, w ktorych cos jest — cisza miedzy zdaniami
        // zanizylaby wartosc skuteczna i odtwarzanie wyszloby za glosne.
        const double peak = peakOf (ownVoice);

        for (float v : ownVoice)
            if (std::fabs (v) > peak * 0.05) { sum += (double) v * v; ++n; }

        if (n < 100) return {};

        const double rms = std::sqrt (sum / (double) n);
        const float  g   = (float) (targetRms / std::max (1.0e-6, rms));

        std::vector<float> out (ownVoice.size());

        for (size_t i = 0; i < ownVoice.size(); ++i)
            out[i] = std::max (-1.0f, std::min (1.0f, ownVoice[i] * g));

        return out;
    }

    /** Poziom, na ktory wyrownano material kalibracyjny — potrzebny do oceny. */
    static constexpr float calibrationRms = 0.05f;

    const SetupReport& result() const noexcept { return report; }
    const std::vector<SetupFinding>& all() const noexcept { return findings; }

private:
    //--------------------------------------------------------------------------
    static std::string db (double v, int d = 1)
    {
        char b[64];
        std::snprintf (b, sizeof (b), "%.*f dBFS", d, v);
        return b;
    }

    double peakDb() const
    {
        float p = 0.0f;
        for (float v : buffer) p = std::max (p, std::fabs (v));
        return p > 1.0e-7f ? 20.0 * std::log10 ((double) p) : -140.0;
    }

    double rmsDb (size_t from, size_t to) const
    {
        double s = 0.0;
        size_t n = 0;

        for (size_t i = from; i < to && i < buffer.size(); ++i) { s += (double) buffer[i] * buffer[i]; ++n; }

        return n && s > 0.0 ? 10.0 * std::log10 (s / n) : -140.0;
    }

    /** Poziomy blokow 50 ms, posortowane — z tego biora sie percentyle. */
    std::vector<double> blockLevels() const
    {
        const int block = std::max (1, (int) (sr * 0.05));
        std::vector<double> out;

        for (size_t s = 0; s + (size_t) block <= buffer.size(); s += (size_t) block)
        {
            double sum = 0.0;
            for (int i = 0; i < block; ++i)
                sum += (double) buffer[s + (size_t) i] * buffer[s + (size_t) i];

            out.push_back (sum > 1.0e-20 ? 10.0 * std::log10 (sum / block) : -140.0);
        }

        return out;
    }

    double percentile (double p) const
    {
        auto lv = blockLevels();
        if (lv.empty()) return -140.0;

        std::sort (lv.begin(), lv.end());
        return lv[std::min (lv.size() - 1, (size_t) (p * (double) (lv.size() - 1)))];
    }

    /** Energia pojedynczego tonu, oknem Hanna. Bez okna sasiednie
        czestotliwosci mierza to samo i przydzwiek jest niewykrywalny. */
    double toneDb (double hz) const
    {
        const int block = std::min ((int) buffer.size(), 32768);
        if (block < 8192) return -140.0;

        const double w = 2.0 * M_PI * hz / sr;
        const double c = 2.0 * std::cos (w);

        double total = 0.0;
        int count = 0;

        for (size_t start = 0; start + (size_t) block <= buffer.size(); start += (size_t) block)
        {
            double s1 = 0.0, s2 = 0.0;

            for (int i = 0; i < block; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (block - 1));
                const double s0 = buffer[start + (size_t) i] * win + c * s1 - s2;
                s2 = s1; s1 = s0;
            }

            total += (s1 * s1 + s2 * s2 - c * s1 * s2) * 4.0 / ((double) block * block);
            ++count;
        }

        return count && total > 1.0e-24 ? 10.0 * std::log10 (total / count) : -140.0;
    }

    double bandDb (double hz, double q) const
    {
        mix::Biquad f;
        f.setCoeffs (mix::design::bandpass (hz, sr, q));

        double s = 0.0;
        size_t n = 0;
        const size_t warm = std::min (buffer.size() / 4, (size_t) (sr * 0.1));

        for (size_t i = 0; i < buffer.size(); ++i)
        {
            const float y = f.process (buffer[i]);
            if (i >= warm) { s += (double) y * y; ++n; }
        }

        return n && s > 0.0 ? 10.0 * std::log10 (s / n) : -140.0;
    }

    //--------------------------------------------------------------------------
    SetupFinding judgeDevice()
    {
        const double pk = peakDb();
        SetupFinding f;

        if (pk < -55.0)
        {
            f.passed = false;
            f.headline = "Nie słyszę nic.";
            f.detail = "Najczęstsza przyczyna: wybrane jest inne wejście niż to, "
                       "do którego wpięty jest mikrofon. Sprawdź też, czy interfejs "
                       "ma włączone zasilanie fantomowe, jeśli mikrofon go potrzebuje.";
        }
        else if (pk < -35.0)
        {
            f.passed = false;
            f.headline = "Słyszę coś, ale bardzo słabo.";
            f.detail = "To może być zły kanał albo wzmocnienie skręcone do zera. "
                       "Podkręć gałkę gain i spróbuj jeszcze raz.";
        }
        else
        {
            f.passed = true;
            f.headline = "Słyszę Cię.";
            f.detail = "To jest właściwe wejście.";
        }

        f.measured = "szczyt " + db (pk);
        return f;
    }

    //--------------------------------------------------------------------------
    SetupFinding judgeRoom()
    {
        report.noiseFloorDb = percentile (0.20);

        // ---- przydzwiek sieciowy --------------------------------------------
        double bestHz = 0.0, bestMargin = 0.0;

        for (double hz : { 50.0, 60.0 })
        {
            const double on = toneDb (hz);
            const double off = std::max (toneDb (hz * 0.88), toneDb (hz * 1.12));

            if (on - off > bestMargin) { bestMargin = on - off; bestHz = hz; }
        }

        if (bestMargin > limits.humMarginDb)
        {
            report.humHz = bestHz;
            report.humExcessDb = bestMargin;
        }

        // ---- czy system majstruje przy sygnale ------------------------------
        // Systemowa redukcja szumu i automatyczne wzmocnienie robia z ciszy cos,
        // czego zaden mikrofon nie produkuje, i oba psuja nam profil glosu.
        // Redukcja: tlo zapada sie po ulamku sekundy, czesto do samego zera.
        // Wzmocnienie: tlo ROSNIE, bo system szuka sygnalu, ktorego nie ma.
        auto lv = blockLevels();

        if (lv.size() >= 8)
        {
            const size_t q = lv.size() / 4;
            double first = 0.0, last = 0.0;

            for (size_t i = 0; i < q; ++i) first += lv[i];
            for (size_t i = lv.size() - q; i < lv.size(); ++i) last += lv[i];

            first /= q; last /= q;

            if (first - last > 15.0 || report.noiseFloorDb < -95.0)
                report.suppressionSuspected = true;

            if (last - first > 8.0)
                report.agcSuspected = true;
        }

        SetupFinding f;
        f.measured = "tło " + db (report.noiseFloorDb);

        if (report.suppressionSuspected)
        {
            f.passed = false;
            f.headline = "Windows majstruje przy dźwięku.";
            f.detail = "Tło zapada się w ciszy — to systemowa redukcja szumu. "
                       "Wyłącz ją: Ustawienia dźwięku → właściwości urządzenia wejściowego → "
                       "Ulepszenia audio → Wyłączone. Inaczej program uczy się algorytmu, "
                       "a nie Twojego mikrofonu.";
        }
        else if (report.agcSuspected)
        {
            f.passed = false;
            f.headline = "Wzmocnienie samo się zmienia.";
            f.detail = "Tło rośnie w ciszy — to automatyczna regulacja wzmocnienia. "
                       "Wyłącz ją w ustawieniach urządzenia; inaczej poziom będzie "
                       "pływał w trakcie całego nagrania.";
        }
        else if (report.humHz > 0.0)
        {
            f.passed = false;
            f.headline = "Słychać przydźwięk z sieci.";
            f.detail = "Zwykle pomaga wypięcie zasilacza laptopa albo przełożenie "
                       "kabla mikrofonowego z dala od zasilaczy i ładowarek. "
                       "Program potrafi to wyciąć przy miksie, ale lepiej, żeby "
                       "tego nie było.";
            f.measured += ", " + std::to_string ((int) report.humHz) + " Hz wystaje o "
                        + db (report.humExcessDb, 0).substr (0, 4) + " dB";
        }
        else if (report.noiseFloorDb > limits.okRoomDb)
        {
            f.passed = false;
            f.headline = "W pokoju jest za głośno.";
            f.detail = "Wyłącz wentylator i klimatyzację, zamknij okno, odsuń komputer. "
                       "Tego szumu nie da się potem usunąć spod mowy bez szkody dla głosu.";
        }
        else if (report.noiseFloorDb > limits.quietRoomDb)
        {
            f.passed = true;
            f.headline = "Da się pracować.";
            f.detail = "Tło jest słyszalne, ale brama sobie z nim poradzi między zdaniami.";
        }
        else
        {
            f.passed = true;
            f.headline = "Cicho jak trzeba.";
            f.detail = "Ten pokój nadaje się do nagrywania.";
        }

        return f;
    }

    //--------------------------------------------------------------------------
    SetupFinding judgeLevel()
    {
        report.speechPeakDb = peakDb();
        report.speechRmsDb = percentile (0.90);
        report.gainAdviceDb = -12.0 - report.speechPeakDb;

        // Efekt zblizeniowy: kazdy mikrofon kierunkowy podbija dol, gdy zrodlo
        // jest blisko. Nadmiar dolu wzgledem srodka pasma mowi o odleglosci
        // lepiej niz jakakolwiek instrukcja "mniej wiecej dlon".
        report.proximityDb = bandDb (110.0, 0.8) - bandDb (900.0, 0.7);

        SetupFinding f;
        f.measured = "szczyty " + db (report.speechPeakDb);

        if (report.speechPeakDb > -1.0)
        {
            f.passed = false;
            f.headline = "Przester.";
            f.detail = "Ścięte próbki nie wracają — żadna wtyczka tego nie naprawi. "
                       "Przykręć wzmocnienie o " + std::to_string ((int) -report.gainAdviceDb)
                     + " dB i powtórz.";
        }
        else if (report.speechPeakDb > limits.peakHighDb)
        {
            f.passed = false;
            f.headline = "Za głośno.";
            f.detail = "Przykręć wzmocnienie o " + std::to_string ((int) -report.gainAdviceDb)
                     + " dB. Zostaw zapas na to, że za chwilę się zaśmiejesz.";
        }
        else if (report.speechPeakDb < limits.peakLowDb)
        {
            f.passed = false;
            f.headline = "Za cicho.";
            f.detail = "Podkręć wzmocnienie o " + std::to_string ((int) report.gainAdviceDb)
                     + " dB albo przysuń się do mikrofonu. Zbyt niski poziom znaczy, "
                       "że razem z głosem podniesiemy potem szum.";
        }
        else if (report.proximityDb > limits.proximityDb)
        {
            f.passed = false;
            f.headline = "Jesteś za blisko.";
            f.detail = "Dół jest podbity o " + std::to_string ((int) report.proximityDb)
                     + " dB ponad normę — to efekt zbliżeniowy. Odsuń się o dłoń; "
                       "głos będzie brzmiał naturalniej i mniej będzie plozyw.";
        }
        else
        {
            f.passed = true;
            f.headline = "Poziom jest dobry.";
            f.detail = "Szczyty w sam raz, odległość w porządku. Nie zmieniaj już gałki.";
        }

        return f;
    }

    //--------------------------------------------------------------------------
    SetupFinding judgePlosive()
    {
        // Uderzenie powietrza w membrane to krotki wystrzal na dole. Porownujemy
        // NAJGORSZY moment dolu z tym, co dol robi przez cala reszte — sam
        // poziom dolu niczego nie dowodzi, bo niski glos ma go duzo zawsze.
        mix::Biquad low;
        low.setCoeffs (mix::design::lowpass (120.0, sr, 0.7071));

        const int win = std::max (1, (int) (sr * 0.03));
        double worst = -140.0, typical = 0.0;
        int blocks = 0;

        std::vector<double> lows;
        double acc = 0.0;
        int n = 0;

        for (size_t i = 0; i < buffer.size(); ++i)
        {
            const float y = low.process (buffer[i]);
            acc += (double) y * y;

            if (++n >= win)
            {
                const double lvl = acc > 1.0e-20 ? 10.0 * std::log10 (acc / n) : -140.0;
                lows.push_back (lvl);
                worst = std::max (worst, lvl);
                acc = 0.0; n = 0; ++blocks;
            }
        }

        if (! lows.empty())
        {
            auto sorted = lows;
            std::sort (sorted.begin(), sorted.end());

            // Porownanie z MEDIANA bylo bledem. W mowie energia dolu skacze o
            // kilkadziesiat decybeli miedzy samogloska a cisza miedzy slowami,
            // wiec mediana lezy gleboko pod tym, co dol robi normalnie —
            // i kazda zwykla samogloska wygladala jak uderzenie. Na czystej
            // mowie z espeaka wychodzilo z tego 18 dB "nadmiaru" bez zadnej
            // plozywy.
            //
            // Plozywa ma wystawac ponad to, co dol robi w swoich NAJGLOSNIEJSZYCH
            // zwyklych momentach — stad dziewiecdziesiaty percentyl.
            typical = sorted[(size_t) (0.90 * (double) (sorted.size() - 1))];
        }

        report.plosiveExcessDb = worst - typical;

        SetupFinding f;
        f.measured = "uderzenie " + std::to_string ((int) report.plosiveExcessDb) + " dB ponad tło";

        if (report.plosiveExcessDb > limits.plosiveDb)
        {
            f.passed = false;
            f.headline = "„P” uderza w membranę.";
            f.detail = "Odchyl mikrofon o piętnaście stopni, tak żebyś mówił wzdłuż niego, "
                       "a nie prosto w niego. Popfiltr albo nawet nylonowa pończocha na "
                       "obręczy załatwia resztę.";
        }
        else
        {
            f.passed = true;
            f.headline = "Plozywy pod kontrolą.";
            f.detail = "Ustawienie mikrofonu jest dobre.";
        }

        return f;
    }

    //--------------------------------------------------------------------------
    SetupFinding judgeBleed()
    {
        // Ile z tego, co gralo w sluchawkach, wrocilo do mikrofonu. Liczymy
        // korelacje przy najlepszym dopasowaniu opoznienia, bo dzwiek ma do
        // przebycia droge przez powietrze.
        double best = 0.0;

        if (! reference.empty() && ! buffer.empty())
        {
            const size_t n = std::min (buffer.size(), reference.size());
            const int maxLag = (int) (sr * 0.02);

            double refEnergy = 0.0;
            for (size_t i = 0; i < n; ++i) refEnergy += (double) reference[i] * reference[i];

            double micEnergy = 0.0;
            for (size_t i = 0; i < n; ++i) micEnergy += (double) buffer[i] * buffer[i];

            if (refEnergy > 1.0e-12 && micEnergy > 1.0e-12)
            {
                for (int lag = 0; lag < maxLag; lag += 8)
                {
                    double dot = 0.0;

                    for (size_t i = 0; i + (size_t) lag < n; ++i)
                        dot += (double) reference[i] * buffer[i + (size_t) lag];

                    best = std::max (best, std::fabs (dot) / std::sqrt (refEnergy * micEnergy));
                }
            }
        }

        report.bleedPercent = best * 100.0;

        SetupFinding f;
        char m[64];
        std::snprintf (m, sizeof (m), "przesłuch %.1f%%", report.bleedPercent);
        f.measured = m;

        if (report.bleedPercent > limits.bleedPercent)
        {
            f.passed = false;
            f.headline = "Słuchawki uciekają do mikrofonu.";
            f.detail = "Ścisz je albo zmień na zamknięte. Przy nagrywaniu do podkładu "
                       "ten przesłuch wejdzie w nagranie i nie da się go potem oddzielić "
                       "od głosu.";
        }
        else
        {
            f.passed = true;
            f.headline = "Słuchawki nie uciekają.";
            f.detail = "Możesz nagrywać do podkładu.";
        }

        return f;
    }

    //--------------------------------------------------------------------------
    /** Kalibracja odsluchu: czy to, co slyszysz, gra tak glosno jak Ty.

        ---------------------------------------------------------------------
        DLACZEGO WLASNY GLOS, A NIE SZUM O ZNANYM POZIOMIE

        Studia kalibruja odsluch szumem rozowym i miernikiem SPL: gra sie
        -20 dBFS i krec, az miernik pokaze 83 dB. To jest dobre i zupelnie
        nieprzydatne komus, kto ma laptop, sluchawki i zadnego miernika.

        Ale kazdy ma jeden wzorzec glosnosci, ktory zna doskonale i nosi ze
        soba: wlasny glos. Wiesz, jak glosno mowisz. Wiec program odtwarza
        Twoje wlasne dwa zdania sprzed chwili i prosi, zebys ustawil galke tak,
        zeby brzmialy jak Ty. Od tej chwili "-16 LUFS" znaczy dla Ciebie cos
        konkretnego.

        ---------------------------------------------------------------------
        CO PROGRAM MOZE ZMIERZYC, A CZEGO NIE

        Na glosnikach: mikrofon slyszy odtwarzanie, wiec da sie porownac je z
        Twoim zywym glosem zmierzonym w kroku "Poziom" — i podac liczbe.

        W sluchawkach: mikrofon nie slyszy nic i zadna liczba nie powstanie.
        Program mowi wtedy wprost, ze ocena nalezy do Ciebie, zamiast wymyslac
        pomiar, ktorego nie ma. To jest krok, ktory ma prawo nie miec wyniku.
    */
    SetupFinding judgeMonitor()
    {
        SetupFinding f;

        // Ten sam pomiar po obu stronach — inaczej porownywaloby sie wartosc
        // skuteczna calego fragmentu (z cisza miedzy zdaniami) z 90. percentylem
        // blokow, i roznica wyszlaby z metody, a nie z glosnikow.
        const double heardDb = percentile (0.90);
        const double floorDb = report.noiseFloorDb;
        const double voiceDb = report.speechRmsDb;

        // Czy mikrofon w ogole cos uslyszal ponad tlo pokoju.
        const bool heard = heardDb > floorDb + 8.0 && voiceDb > -130.0;

        report.monitorMeasured = heard;

        if (! heard)
        {
            report.monitorVsVoiceDb = 0.0;

            f.passed = true;
            f.headline = "Ustawione. Zapamiętaj tę głośność.";
            f.detail = "Mikrofon nie słyszał odsłuchu, więc masz słuchawki — i tej "
                       "głośności nie zmierzę. Ocena należy do Ciebie i była prosta: "
                       "miało brzmieć jak Ty. Od tej pory, gdy program powie „za cicho”, "
                       "będziesz wiedział, względem czego.";
            f.measured = "słuchawki — bez pomiaru";
            return f;
        }

        // Odtwarzanie bylo wyrownane do tej samej wartosci skutecznej, wiec
        // roznica poziomow przy mikrofonie to roznica glosnosci w pokoju.
        report.monitorVsVoiceDb = heardDb - voiceDb;

        char m[80];
        std::snprintf (m, sizeof (m), "odsłuch %+.1f dB względem Twojego głosu",
                       report.monitorVsVoiceDb);
        f.measured = m;

        if (report.monitorVsVoiceDb > 6.0)
        {
            f.passed = false;
            f.headline = "Głośniki grają głośniej, niż mówisz.";
            f.detail = "Przy takim odsłuchu każde nagranie wyda Ci się za głośne, "
                       "a Ty będziesz je ściszał w miksie — i u słuchacza wyjdzie za cicho. "
                       "Skręć, aż nagranie będzie brzmiało jak rozmowa w tym pokoju.";
        }
        else if (report.monitorVsVoiceDb < -8.0)
        {
            f.passed = false;
            f.headline = "Głośniki grają dużo ciszej, niż mówisz.";
            f.detail = "Przy cichym odsłuchu nie słychać ani szumu tła, ani plozyw — "
                       "usłyszysz je dopiero u kogoś innego. Podgłośnij.";
        }
        else
        {
            f.passed = true;
            f.headline = "Odsłuch ustawiony.";
            f.detail = "Twoje nagranie gra mniej więcej tak głośno, jak Ty mówisz. "
                       "To jest punkt zerowy: od teraz liczby, które podaję, mają dla "
                       "Ciebie odpowiednik w tym, co słyszysz. Nie zmieniaj już gałki.";
        }

        return f;
    }

    static double peakOf (const std::vector<float>& v)
    {
        double m = 0.0;
        for (float x : v) m = std::max (m, (double) std::fabs (x));
        return m;
    }

    //--------------------------------------------------------------------------
    double sr = 48000.0;
    SetupStep current = SetupStep::Device;
    std::vector<float> buffer, reference;

    /** Glos czlowieka z kroku "Poziom", zachowany na krok kalibracji odsluchu.
        To jest jedyny dzwiek na swiecie, ktorego glosnosc kazdy zna na pamiec. */
    std::vector<float> ownVoice;

    std::vector<SetupFinding> findings;
    SetupReport report;
};

} // namespace tw
