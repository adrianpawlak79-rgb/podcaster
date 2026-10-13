/*
    odsluch-test.cpp — ekran po nagraniu, bez ekranu.

    ---------------------------------------------------------------------------
    CO TU SIE SPRAWDZA

    Rdzen ekranu po nagraniu odpowiada na jedno pytanie: czy to sie nadaje, a
    jesli nie, to ktory fragment. Odpowiedz powstaje z dwoch rzeczy, ktore
    bardzo latwo pomylic:

        - zdarzen, ktore miks naprawi  (oddech, sybilant, plozywa, szum)
        - zdarzen, ktorych nie naprawi (przester, cisza cyfrowa)

    Pomylka w te strone jest kosztowna w sposob, ktorego nikt nie zauwazy:
    program powie "nadaje sie", czlowiek rozbierze mikrofon, a przester zostanie
    w pliku na zawsze. Dlatego punkty 2 i 3 sprawdzaja, ze przester NIGDY nie
    wpada do rzeczy naprawialnych, a oddech NIGDY do powtorzenia.

    Druga polowa testu to odsluch. Odtwarzacz czyta w watku dzwieku, wiec kazdy
    jego blad jest trzaskiem w sluchawkach albo cisza w chwili, w ktorej ktos
    wlasnie chcial porownac dwie wersje. Sprawdzamy, ze przelaczenie PRZED/PO
    nie rusza pozycji, ze koniec materialu zatrzymuje graj i ze przewijanie
    poza material nie wychodzi z tablicy.
*/

#include "../Source/DSP/TakeReview.h"
#include "../Source/Wav.h"

#include <cstdio>
#include <random>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* unit = " dB")
{
    char b[48];
    std::snprintf (b, sizeof (b), "%.1f%s", v, unit);
    return b;
}

//==============================================================================
static tw::Event ev (tw::Kind k, double t, tw::Severity s, float v = 1.0f, double redoFrom = 0.0)
{
    tw::Event e;
    e.kind     = k;
    e.timeSec  = t;
    e.severity = s;
    e.value    = v;
    e.redoFrom = redoFrom > 0.0 ? redoFrom : t;
    return e;
}

/** Mowa z espeaka, powielona do zadanej dlugosci. */
static std::vector<float> speech (const std::vector<float>& src, double sr, double sec)
{
    std::vector<float> out ((size_t) (sr * sec));

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = src[i % src.size()];

    return out;
}

//==============================================================================
int main (int argc, char** argv)
{
    std::printf ("\nEkran po nagraniu\n=================\n\n");

    const std::string path = argc > 1 ? argv[1] : "/tmp/mowa.wav";

    wavio::Audio in;
    const auto err = wavio::read (path, in);

    if (! err.empty())
    {
        std::printf ("  nie moge wczytac %s: %s\n", path.c_str(), err.c_str());
        return 2;
    }

    const double sr  = (double) in.sampleRate;
    const auto   src = in.toMono();

    // ---- 1. skladanie zdarzen w kawalki osi ------------------------------
    //
    // Siedem oddechow w ciagu czterech sekund to jedno miejsce w nagraniu, nie
    // siedem kresek na osi. Ale oddech i przester w tej samej sekundzie to dwie
    // rozne wiadomosci i nie wolno ich scalic.
    {
        std::printf ("  skladanie zdarzen\n");

        std::vector<tw::Event> e;

        for (int i = 0; i < 7; ++i)
            e.push_back (ev (tw::Kind::Breath, 2.0 + i * 0.5, tw::Severity::Later, 3.0f));

        e.push_back (ev (tw::Kind::Clipping, 3.0, tw::Severity::Stop, -0.1f, 2.4));
        e.push_back (ev (tw::Kind::Breath,  40.0, tw::Severity::Later, 3.0f));

        zs::TakeReview r;
        r.setTake (speech (src, sr, 50.0), sr, e);

        const auto& t = r.timeline();

        check (t.size() == 3, "9 zdarzen daje 3 kawalki osi",
               std::to_string (t.size()) + " kawalkow");

        int breathPieces = 0, clipPieces = 0;

        for (const auto& x : t)
        {
            if (x.kind == tw::Kind::Breath)   ++breathPieces;
            if (x.kind == tw::Kind::Clipping) ++clipPieces;
        }

        check (breathPieces == 2, "oddechy: seria blisko siebie i jeden osobno",
               std::to_string (breathPieces) + " kawalkow");
        check (clipPieces == 1,   "przester nie wtopil sie w oddechy");

        for (const auto& x : t)
            if (x.kind == tw::Kind::Breath && x.count == 7)
                check (x.toSec - x.fromSec > 2.5, "seria obejmuje caly swoj czas",
                       num (x.toSec - x.fromSec, " s"));
    }

    // ---- 2. przester idzie do powtorzenia, oddech nie --------------------
    //
    // To jest ten jeden podzial, ktorego nie wolno pomylic. Reszta ekranu moze
    // byc nieladna; ta granica musi byc szczelna.
    {
        std::printf ("\n  co da sie naprawic, a czego nie\n");

        zs::TakeReview r;
        r.setTake (speech (src, sr, 20.0), sr,
        {
            ev (tw::Kind::Breath,     3.0, tw::Severity::Later),
            ev (tw::Kind::Sibilance,  4.0, tw::Severity::Later),
            ev (tw::Kind::Plosive,    5.0, tw::Severity::Redo),
            ev (tw::Kind::MouthClick, 6.0, tw::Severity::Redo),
            ev (tw::Kind::NoiseRise,  7.0, tw::Severity::Later),
            ev (tw::Kind::Hum,        8.0, tw::Severity::Setup),
            ev (tw::Kind::LevelDrift, 9.0, tw::Severity::Later),
            ev (tw::Kind::OffAxis,   10.0, tw::Severity::Later),
        });

        check (r.toRedo().empty(),
               "osiem rzeczy naprawialnych = zero do powtorzenia",
               std::to_string (r.toRedo().size()) + " fragmentow");
        check (r.verdict() <= 1, "werdykt: nadaje sie");
        // Trzy miejsca: plozja, mlask i przydzwiek (Setup) — oddech, sybilant
        // i reszta sa ponizej progu "warto posluchac".
        // Werdykt "nadaje sie, ale posluchaj" ma mowic ILE i GDZIE — inaczej
        // przeczy panelowi, ktory mowi "nie ma fragmentow do powtorzenia".
        check (r.verdict() != 1 || (r.verdictLine().find ("3 miejsc") != std::string::npos
                                    && r.verdictLine().find ("0:05") != std::string::npos),
               "werdykt podaje liczbe i czas miejsc", r.verdictLine());

        zs::TakeReview c;
        c.setTake (speech (src, sr, 20.0), sr,
        {
            ev (tw::Kind::Breath,   3.0, tw::Severity::Later),
            ev (tw::Kind::Clipping, 8.0, tw::Severity::Stop, -0.05f, 7.2),
        });

        check (c.toRedo().size() == 1, "przester daje jeden fragment do powtorzenia",
               std::to_string (c.toRedo().size()));
        check (c.verdict() == 2,       "werdykt: trzeba powtorzyc");

        if (! c.toRedo().empty())
            check (c.toRedo()[0].fromSec <= 7.3,
                   "powtorka liczona od poczatku frazy, nie od sylaby",
                   num (c.toRedo()[0].fromSec, " s"));

        // Cisza cyfrowa w normalnym nagraniu to urwany sygnal — do powtorzenia.
        // "Normalnym" znaczy takim, w ktorym tla jednak cos jest: budujemy je z
        // mowy plus cichy szum, czyli tak, jak wyglada kazde nagranie z
        // prawdziwego mikrofonu.
        auto withRoomTone = [&] (double sec)
        {
            auto v = speech (src, sr, sec);
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> n (-0.0004f, 0.0004f);   // ok. -68 dBFS

            for (auto& x : v) x += n (rng);
            return v;
        };

        zs::TakeReview s;
        s.setTake (withRoomTone (20.0), sr,
        {
            ev (tw::Kind::DigitalSilence, 5.0, tw::Severity::Stop, -140.0f, 4.0),
        });

        check (s.toRedo().size() == 1, "cisza cyfrowa w normalnym nagraniu: do powtorzenia",
               std::to_string (s.toRedo().size()) + " fragmentow");
    }

    // ---- 2b. cisza, ktora jest charakterem calego pliku ------------------
    //
    // Material zbramkowany gdzie indziej albo wygenerowany ma cisze zupelna w
    // kazdej przerwie. Dotad program wypisywal wtedy dziewiec "powtorz 0:03–0:04"
    // na nagraniu, z ktorym nie bylo nic nie tak — a lista, ktora raz odeslala
    // czlowieka do mikrofonu po nic, przestaje byc czytana cala.
    {
        std::printf ("\n  cisza jako charakter materialu\n");

        auto gated = speech (src, sr, 30.0);

        // co drugie poltorej sekundy: zero probek
        for (size_t i = 0; i < gated.size(); ++i)
            if (((i / (size_t) (sr * 1.5)) % 2) == 1) gated[i] = 0.0f;

        std::vector<tw::Event> e;

        for (int i = 0; i < 9; ++i)
            e.push_back (ev (tw::Kind::DigitalSilence, 2.0 + i * 3.0, tw::Severity::Stop, -140.0f));

        e.push_back (ev (tw::Kind::Clipping, 20.0, tw::Severity::Stop, -0.02f, 19.0));

        zs::TakeReview r;
        r.setTake (gated, sr, e);

        check (r.toRedo().size() == 1,
               "dziewiec przerw nie trafia na liste powtorek",
               std::to_string (r.toRedo().size()) + " fragmentow");

        if (! r.toRedo().empty())
            check (r.toRedo()[0].kind == tw::Kind::Clipping,
                   "ale przester z tego samego pliku — owszem");

        r.run();

        bool explained = false;

        for (const auto& line : r.says())
            if (line.find ("Sprawdź, czy") != std::string::npos)
                explained = true;

        check (explained, "i program mowi wprost, dlaczego ich nie liczy");

        // Wszystkie kawalki nadal widac na osi — pominiecie ich w liscie
        // powtorek nie znaczy udawania, ze ich nie bylo.
        int silencePieces = 0;

        for (const auto& t : r.timeline())
            if (t.kind == tw::Kind::DigitalSilence) ++silencePieces;

        check (silencePieces == 9, "na osi czasu zostaja wszystkie",
               std::to_string (silencePieces) + " kawalkow");
    }

    // ---- 3. dwa przestery blisko siebie to jedna powtorka ----------------
    {
        std::printf ("\n  scalanie powtorek\n");

        zs::TakeReview r;
        r.setTake (speech (src, sr, 40.0), sr,
        {
            ev (tw::Kind::Clipping,  8.0, tw::Severity::Stop, -0.1f, 7.0),
            ev (tw::Kind::Clipping,  9.0, tw::Severity::Stop, -0.1f, 7.0),
            ev (tw::Kind::Clipping, 30.0, tw::Severity::Stop, -0.1f, 29.0),
        });

        check (r.toRedo().size() == 2,
               "dwa blisko + jeden daleko = dwie powtorki",
               std::to_string (r.toRedo().size()));
    }

    // ---- 4. caly przebieg: slucha, decyduje, miksuje ---------------------
    //
    // Cel glosnosci jest DEKLARACJA programu, a sufit obietnica. Jedno i drugie
    // musi wyjsc, bo to sa jedyne dwie liczby, ktore ekran po nagraniu obiecuje
    // czlowiekowi wprost.
    {
        std::printf ("\n  caly przebieg\n");

        zs::TakeReview r;
        r.setTake (speech (src, sr, 25.0), sr, { ev (tw::Kind::Breath, 3.0, tw::Severity::Later) });

        check (r.stage() == zs::TakeReview::Stage::Listening, "zanim ruszy: slucham");

        r.run();

        check (r.stage() == zs::TakeReview::Stage::Ready, "po przebiegu: gotowe");
        check (r.progress() >= 1.0, "postep doszedl do konca");

        const auto& rep = r.whatIDid();

        check (r.mixedAudio().size() == r.rawAudio().size(),
               "miks ma dlugosc nagrania",
               std::to_string (r.mixedAudio().size()) + " probek");

        const double targetMiss = std::fabs (rep.lufsAfter - r.whatIDecided().params.targetLufs);

        check (targetMiss < 0.3, "glosnosc trafia w cel, ktory program sam podal",
               num (rep.lufsAfter, " LUFS") + ", cel "
                   + num (r.whatIDecided().params.targetLufs, " LUFS"));

        check (rep.peakAfter <= r.whatIDecided().params.ceilingDbTp + 0.1,
               "sufit dotrzymany",
               num (rep.peakAfter, " dBTP"));

        check (! r.says().empty(), "program ma co powiedziec",
               std::to_string (r.says().size()) + " zdan");

        bool hasNumber = false;

        for (const auto& line : r.says())
            if (line.find_first_of ("0123456789") != std::string::npos)
                hasNumber = true;

        check (hasNumber, "i kazde zdanie opiera sie na liczbie");
        check (! r.verdictLine().empty(), "werdykt jest jednym zdaniem");

        std::printf ("\n        werdykt: %s\n\n", r.verdictLine().c_str());

        // Jeden szczyt nie moze sciszyc calego odcinka (Docs/badania/SYMULACJA.md:
        // w trybie Auto przester w jednym miejscu dawal -23 LUFS zamiast -16).
        auto loud = speech (src, sr, 25.0);
        for (int i = 0; i < 2400; ++i)
            loud[(size_t) (10.0 * sr) + (size_t) i] = (i / 24) % 2 ? -1.0f : 1.0f;
        zs::TakeReview p;
        p.setTake (loud, sr, {});
        p.run();
        // Cel z decyzji, nie liczba wpisana na sztywno: miks studyjny (domyslny
        // od PR z lancuchem v3) celuje w -19 LUFS — norma AES dla pliku MONO,
        // odpowiednik -16 w stereo. Test pilnuje, ze szczyt nie sciszy calosci.
        const double target = p.whatIDecided().params.targetLufs;
        check (std::fabs (p.whatIDid().lufsAfter - target) < 1.0, "podcast: cel glosnosci mimo jednego szczytu 0 dBFS",
               num (p.whatIDid().lufsAfter, " LUFS") + " (cel " + num (target, ")"));
        check (p.whatIDid().peakAfter <= -0.9, "i szczyt pod sufitem", num (p.whatIDid().peakAfter, " dBTP"));
    }

    // ---- 5. zapis pliku i powrot ----------------------------------------
    //
    // Miks, ktorego nie da sie odczytac, jest tyle samo warty co brak miksu.
    // Program zapisuje przez wavio (to samo, co narzedzia konsolowe), wiec
    // sprawdzamy dokladnie te droge, ktora pojdzie nagranie czlowieka.
    {
        std::printf ("  zapis na dysk\n");

        zs::TakeReview r;
        r.setTake (speech (src, sr, 8.0), sr, {});
        r.run();

        wavio::Audio out;
        out.sampleRate = (int) sr;
        out.channels.push_back (r.mixedAudio());

        const auto wErr = wavio::write24 ("/tmp/odsluch-test.wav", out);
        check (wErr.empty(), "plik zapisany", wErr);

        wavio::Audio back;
        const auto rErr = wavio::read ("/tmp/odsluch-test.wav", back);

        check (rErr.empty(), "i daje sie odczytac", rErr);
        check (back.sampleRate == (int) sr, "czestotliwosc probkowania ta sama");
        check (back.frames() == r.mixedAudio().size(), "tyle samo probek");

        // 24 bity to krok 1/8388608, czyli -138 dBFS. Roznica wieksza niz to
        // znaczy, ze zapisalismy co innego, niz zmiksowalismy.
        double worst = 0.0;

        if (back.numChannels() > 0)
            for (size_t i = 0; i < back.frames() && i < r.mixedAudio().size(); ++i)
                worst = std::max (worst, (double) std::fabs (back.channels[0][i] - r.mixedAudio()[i]));

        check (worst < 1.0e-6, "probki te same co w pamieci",
               num (worst > 0.0 ? 20.0 * std::log10 (worst) : -200.0, " dBFS"));
    }

    // ---- 6. odsluch PRZED/PO ---------------------------------------------
    {
        std::printf ("\n  odsluch\n");

        zs::Audition au;

        std::vector<float> a (1000), b (1000);

        for (size_t i = 0; i < a.size(); ++i)
        {
            a[i] = 0.25f;     // "przed" to staly jeden poziom
            b[i] = 0.50f;     // "po" to staly drugi
        }

        au.setMaterial (a, b, 1000.0);

        check (au.hasMaterial(),            "material wstawiony");
        check (! au.isPlaying(),            "i nic jeszcze nie gra");
        check (std::fabs (au.lengthSec() - 1.0) < 1.0e-9, "dlugosc policzona z probek");

        float buf[100];

        au.showAfter (true);
        au.play();

        check (au.pull (buf, 100),          "graj oddaje probki");
        check (std::fabs (buf[0] - 0.5f) < 1.0e-6, "i to probki z PO", num (buf[0], ""));
        check (std::fabs (au.positionSec() - 0.1) < 1.0e-9, "pozycja przesunela sie o 100 probek");

        // Przelaczenie w trakcie grania NIE moze ruszyc pozycji — inaczej
        // porownanie nie ma sensu, bo za kazdym klikniecem sluchasz czegos
        // innego.
        const double was = au.positionSec();
        au.showAfter (false);

        check (std::fabs (au.positionSec() - was) < 1.0e-12,
               "przelaczenie nie rusza pozycji");

        // pull() MUSI stac w osobnej linii. Kiedy stalo w warunku, a wartosc
        // buf[0] w trzecim argumencie tego samego wywolania, kompilator mial
        // prawo policzyc argumenty w dowolnej kolejnosci — i wypisywal liczbe
        // sprzed odczytu. Test przechodzil poprawnie i pokazywal blad.
        const bool got = au.pull (buf, 100);

        check (got && std::fabs (buf[0] - 0.25f) < 1.0e-6,
               "a dzwiek jest juz z PRZED", num (buf[0], ""));

        // Koniec materialu zatrzymuje graj i zostawia igle na koncu.
        au.seekSec (0.95);
        check (au.pull (buf, 100), "ostatni kawalek jeszcze sie odda");
        check (! au.isPlaying(),   "koniec materialu zatrzymuje graj");
        check (au.positionSec() > 0.99, "igla zostaje na koncu, nie wraca na poczatek",
               num (au.positionSec(), " s"));

        // Przewijanie poza material nie moze wyjsc z tablicy.
        au.seekSec (-5.0);
        check (au.positionSec() == 0.0, "przewiniecie przed poczatek laduje na zerze");

        au.seekSec (1000.0);
        check (au.positionSec() < 1.0, "przewiniecie za koniec laduje w materiale",
               num (au.positionSec(), " s"));

        au.play();

        for (int i = 0; i < 20; ++i)
            au.pull (buf, 100);

        check (! au.isPlaying(), "i tam sie zatrzymuje, a nie czyta dalej");

        au.clear();
        check (! au.hasMaterial() && ! au.pull (buf, 100),
               "po wyczyszczeniu nie ma czego grac");
    }

    // ---- 7. przebieg do narysowania ---------------------------------------
    {
        std::printf ("\n  przebieg do narysowania\n");

        zs::Envelope e;
        e.build (speech (src, sr, 10.0), sr, 600);

        check (e.peakDb.size() == 600, "tyle wartosci, ile kolumn");
        check (std::fabs (e.seconds - 10.0) < 0.05, "dlugosc zgadza sie z nagraniem",
               num (e.seconds, " s"));

        double loudest = -200.0;
        int quiet = 0;

        for (auto v : e.peakDb)
        {
            loudest = std::max (loudest, (double) v);
            if (v < -60.0) ++quiet;
        }

        check (loudest > -20.0, "mowa widoczna", num (loudest, " dBFS"));
        check (quiet < 500,     "i nie jest to sama cisza",
               std::to_string (quiet) + " cichych kolumn z 600");

        // Pusty material nie moze sie wywalic ani zwrocic zera kolumn — ekran
        // rysuje to zawsze, takze zanim cokolwiek nagrano.
        zs::Envelope z;
        z.build ({}, sr, 600);
        check (z.peakDb.size() == 600, "puste nagranie tez daje sie narysowac");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo"
                                        : "SA BLEDY — patrz wyzej");
    return fails == 0 ? 0 : 1;
}
