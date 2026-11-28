/*
    glos-stereo-test.cpp — zapis bez podkladu: 2 kanaly, -16 LUFS, <= -1 dBTP.

    Decyzja wlasciciela (listopad 2026): nagranie bez muzyki idzie do pliku
    jako stereo z tym samym sygnalem w obu kanalach. Tor liczy w mono do
    -19 LUFS; dwie kopie kanalu to wedlug BS.1770 +3,01 dB, czyli -16 LUFS w
    pliku. Szczyt miedzyprobkowy liczy sie na kanal, wiec sufit zostaje.

    Sprawdzamy na pliku, ktory przeszedl przez ten sam zapis co program
    (wavio::writePcm + mix::dualMonoChannels, jak ReviewComponent::save), a
    glosnosc mierzymy dwa razy: naszym miernikiem i — gdy jest — ffmpeg ebur128.
*/

#include "../Source/DSP/TakeReview.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* unit)
{
    char b[48];
    std::snprintf (b, sizeof (b), "%.2f%s", v, unit);
    return b;
}

int main (int argc, char** argv)
{
    std::printf ("\nZapis bez podkladu: stereo -16 LUFS\n===================================\n\n");

    const std::string path = argc > 1 ? argv[1] : "/tmp/mowa.wav";
    wavio::Audio in;
    const auto err = wavio::read (path, in);
    if (! err.empty() || in.numChannels() == 0)
    {
        std::printf ("  BLAD  brak nagrania %s: %s\n", path.c_str(), err.c_str());
        return 1;
    }

    const double sr = in.sampleRate;
    const auto src = in.toMono();
    std::vector<float> take ((size_t) (sr * 30.0));
    for (size_t i = 0; i < take.size(); ++i) take[i] = src[i % src.size()];

    zs::TakeReview r;
    r.setTake (std::move (take), sr, {});
    r.run();

    const auto& mixed = r.mixedAudio();
    check (r.whatIDecided().params.targetLufs == -19.0, "tor liczy w mono do -19 LUFS",
           num (r.whatIDecided().params.targetLufs, " LUFS"));

    const std::string out = "/tmp/glos-stereo-test.wav";
    const auto wErr = wavio::writePcm (out, mix::dualMonoChannels (mixed), mixed.size(), (int) sr, 24);
    check (wErr.empty(), "plik zapisany", wErr);

    wavio::Audio back;
    const auto rErr = wavio::read (out, back);
    check (rErr.empty(), "i daje sie odczytac", rErr);
    check (back.numChannels() == 2, "2 kanaly", std::to_string (back.numChannels()));
    if (back.numChannels() != 2) { std::printf ("\nZLE: %d\n", fails); return 1; }

    check (back.channels[0] == back.channels[1], "w obu kanalach ten sam sygnal");

    const float* ch[2] = { back.channels[0].data(), back.channels[1].data() };
    mix::LoudnessMeter m; m.prepare (sr, 2); m.process (ch, 2, (int) back.frames());
    mix::TruePeak tp;    tp.prepare (sr, 2); tp.process (ch, 2, (int) back.frames()); tp.finish();
    const double lufs = m.integrated(), peak = tp.dbTP();

    check (std::fabs (lufs + 16.0) <= 0.5, "glosnosc pliku -16 +/-0,5 LUFS (nasz miernik)", num (lufs, " LUFS"));
    check (peak <= -1.0 + 0.05, "szczyt pliku <= -1 dBTP (nasz miernik)", num (peak, " dBTP"));
    check (std::fabs (mix::dualMonoLufs (r.whatIDid().lufsAfter) - lufs) < 0.1,
           "ekran (mono + 3,01) pokazuje to, co w pliku",
           num (mix::dualMonoLufs (r.whatIDid().lufsAfter), " LUFS"));

    // niezalezny pomiar: ffmpeg ebur128 (gdy go nie ma, mowimy to wprost)
    if (std::system ("ffmpeg -version > /dev/null 2>&1") == 0)
    {
        const std::string cmd = "ffmpeg -hide_banner -nostats -i " + out
            + " -af ebur128=peak=true -f null - 2>&1 | grep -A14 Summary";
        FILE* f = popen (cmd.c_str(), "r");
        double I = 0.0, TP = 0.0;
        bool gotI = false, gotTP = false, inTp = false;
        char line[256];
        while (f != nullptr && std::fgets (line, sizeof (line), f))
        {
            const std::string s (line);
            if (s.find ("True peak") != std::string::npos) inTp = true;
            const auto i = s.find ("I:");
            if (i != std::string::npos && ! gotI) gotI = std::sscanf (s.c_str() + i + 2, "%lf", &I) == 1;
            const auto p = s.find ("Peak:");
            if (inTp && p != std::string::npos && ! gotTP) gotTP = std::sscanf (s.c_str() + p + 5, "%lf", &TP) == 1;
        }
        if (f) pclose (f);
        check (gotI && std::fabs (I + 16.0) <= 0.5, "ffmpeg ebur128: -16 +/-0,5 LUFS", num (I, " LUFS"));
        check (gotTP && TP <= -1.0 + 0.1, "ffmpeg ebur128: szczyt <= -1 dBTP", num (TP, " dBFS TP"));
    }
    else
        std::printf ("  (ffmpeg nie ma — pomiar niezalezny pominiety)\n");

    std::printf ("\n%s\n", fails == 0 ? "wszystko OK" : ("ZLE: " + std::to_string (fails)).c_str());
    return fails == 0 ? 0 : 1;
}
