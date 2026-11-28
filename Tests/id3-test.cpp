/*
    id3-test.cpp — tag ID3v2.4 w MP3 (Source/Id3.h).

    Co pilnuje:
      1. zapis -> odczyt: TIT2, TPE1, TRCK, TDRC, COMM wracaja bez zmian,
         razem z polskimi literami (UTF-8);
      2. rozmiary synchsafe: naglowek i ramki nie maja bitu 7, a rozmiar z
         naglowka zgadza sie z dlugoscia tagu (takze dla tagu > 127 bajtow);
      3. stary tag ID3v2 na poczatku jest zastapiony, a dzwiek za nim nietkniety;
      4. numer odcinka z nazwy ("Odcinek 12", "#7");
      5. prawdziwy MP3 (ffmpeg): po dopisaniu tagu ffprobe czyta tytul, autora,
         numer, rok i komentarz, a dlugosc dzwieku sie nie zmienia.

    Uzycie: id3-test [nagranie.wav]   (bez ffmpeg/ffprobe pkt 5 jest pomijany)
*/

#include "../Source/Id3.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::vector<uint8_t> readAll (const std::string& p)
{
    std::ifstream f (p, std::ios::binary);
    return { std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char>() };
}

static void writeAll (const std::string& p, const std::vector<uint8_t>& d)
{
    std::ofstream f (p, std::ios::binary);
    f.write (reinterpret_cast<const char*> (d.data()), (std::streamsize) d.size());
}

static std::string run (const std::string& cmd)
{
    std::string out;
    if (FILE* p = popen (cmd.c_str(), "r"))
    {
        char buf[512];
        while (std::fgets (buf, sizeof (buf), p)) out += buf;
        pclose (p);
    }
    while (! out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

int main (int argc, char** argv)
{
    using namespace zs::id3;

    Tags t;
    t.title   = "Odcinek 12 – Zażółć gęślą jaźń";
    t.artist  = "Anna Kowalska";
    t.episode = "12";
    t.year    = "2026";
    t.comment = "Nagrano w Zielone światło 1.2.3";

    // ---- 1. zapis -> odczyt ----------------------------------------------------
    const auto tag = build (t);
    auto back = parse (tag);
    check (back.size() == 5, "pięć ramek", std::to_string (back.size()));
    check (back["TIT2"] == t.title, "TIT2 (tytuł z polskimi literami)", back["TIT2"]);
    check (back["TPE1"] == t.artist, "TPE1 (autor)", back["TPE1"]);
    check (back["TRCK"] == "12", "TRCK (numer odcinka)", back["TRCK"]);
    check (back["TDRC"] == "2026", "TDRC (rok)", back["TDRC"]);
    check (back["COMM"] == t.comment, "COMM (komentarz)", back["COMM"]);
    check (tag.size() > 10 && tag[0] == 'I' && tag[1] == 'D' && tag[2] == '3' && tag[3] == 4 && tag[4] == 0,
           "nagłówek: ID3 v2.4.0");
    check (tag[10] == 'T' && tag[11] == 'I' && tag[12] == 'T' && tag[13] == '2' && tag[20] == 3,
           "pierwsza ramka TIT2, kodowanie UTF-8");

    Tags only; only.title = "Sam tytuł";
    const auto small = parse (build (only));
    check (small.size() == 1 && small.count ("TIT2") == 1, "puste pola bez ramek", std::to_string (small.size()));
    check (build (Tags{}).empty(), "bez pól nie ma tagu");

    // ---- 2. synchsafe ----------------------------------------------------------
    {
        bool clean = true;
        for (int i = 6; i < 10; ++i) clean = clean && (tag[(size_t) i] & 0x80) == 0;
        const size_t declared = detail::getSynchsafe (&tag[6]);
        check (clean, "rozmiar w nagłówku bez bitu 7");
        check (declared + 10 == tag.size(), "rozmiar z nagłówka = długość tagu",
               std::to_string (declared + 10) + " / " + std::to_string (tag.size()));

        Tags big = t; big.comment = std::string (400, 'x');   // 400 = 0x190: drugi bajt synchsafe nie jest 0
        const auto bigTag = build (big);
        const size_t declaredBig = detail::getSynchsafe (&bigTag[6]);
        check (declaredBig + 10 == bigTag.size() && bigTag.size() > 400, "tag > 400 B: rozmiar się zgadza",
               std::to_string (bigTag.size()));
        check (parse (bigTag)["COMM"] == big.comment, "tag > 400 B: komentarz wraca");

        std::vector<uint8_t> v;
        detail::putSynchsafe (v, 0x0fffffff);
        check (v == std::vector<uint8_t> { 0x7f, 0x7f, 0x7f, 0x7f }, "synchsafe: maksimum 2^28-1");
        v.clear(); detail::putSynchsafe (v, 128);
        check (v == std::vector<uint8_t> { 0, 0, 1, 0 }, "synchsafe: 128 = 00 00 01 00");
    }

    // ---- 3. zastąpienie starego tagu -------------------------------------------
    {
        const std::vector<uint8_t> audio { 0xff, 0xfb, 0x90, 0x00, 1, 2, 3, 4, 5 };
        Tags old; old.title = "Stary tytuł";
        auto first = build (old);
        first.insert (first.end(), audio.begin(), audio.end());
        const auto second = withTag (first, t);
        check (parse (second)["TIT2"] == t.title, "nowy tag zastępuje stary");
        check (second.size() == tag.size() + audio.size()
               && std::equal (audio.begin(), audio.end(), second.end() - (std::ptrdiff_t) audio.size()),
               "dźwięk za tagiem bez zmian");
        const auto bare = withTag (audio, t);
        check (bare.size() == tag.size() + audio.size(), "plik bez tagu: tag doklejony z przodu");
    }

    // ---- 4. numer odcinka ------------------------------------------------------
    check (episodeFromName ("Odcinek 12") == "12", "\"Odcinek 12\"", episodeFromName ("Odcinek 12"));
    check (episodeFromName ("moj odcinek 7 - gosc") == "7", "\"odcinek 7\" w środku");
    check (episodeFromName ("ODCINEK_05") == "5", "\"ODCINEK_05\" -> 5");
    check (episodeFromName ("Podcast #3") == "3", "\"#3\"");
    check (episodeFromName ("Podcast # 3") == "3", "\"# 3\"");
    check (episodeFromName ("Nagranie 2026-11-28").empty(), "sama data to nie numer");
    check (episodeFromName ("Odcinek pilotażowy").empty(), "\"Odcinek\" bez cyfry");
    check (episodeFromName ("Poodcinek 4").empty(), "\"Poodcinek 4\" to nie odcinek");

    // ---- 5. prawdziwy MP3 i ffprobe --------------------------------------------
    const std::string wav = argc > 1 ? argv[1] : "";
    const std::string dir = "/tmp/zs-id3-test";
    if (wav.empty() || run ("command -v ffmpeg").empty() || run ("command -v ffprobe").empty())
        std::printf ("  (pominięto: brak ffmpeg/ffprobe albo nagrania)\n");
    else
    {
        run ("mkdir -p " + dir);
        const std::string mp3 = dir + "/plain.mp3", tagged = dir + "/tagged.mp3";
        run ("ffmpeg -hide_banner -loglevel error -y -i '" + wav + "' -t 3 -c:a libmp3lame -b:a 128k '" + mp3 + "'");
        const auto raw = readAll (mp3);
        const size_t oldTag = existingTagSize (raw.data(), raw.size());   // ffmpeg sam dopisuje TSSE
        check (raw.size() > 1000, "ffmpeg zrobił MP3", std::to_string (raw.size()) + " B, własny tag ffmpeg: " + std::to_string (oldTag) + " B");
        const auto durBefore = run ("ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 '" + mp3 + "'");

        writeAll (tagged, withTag (raw, t));
        const auto probe = [&] (const char* key)
        {
            return run (std::string ("ffprobe -v error -show_entries format_tags=") + key
                        + " -of default=nw=1:nk=1 '" + tagged + "'");
        };
        check (probe ("title") == t.title, "ffprobe: title", probe ("title"));
        check (probe ("artist") == t.artist, "ffprobe: artist", probe ("artist"));
        check (probe ("track") == "12", "ffprobe: track", probe ("track"));
        check (probe ("date") == "2026", "ffprobe: date", probe ("date"));
        check (probe ("comment") == t.comment, "ffprobe: comment", probe ("comment"));
        const auto durAfter = run ("ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 '" + tagged + "'");
        check (! durBefore.empty() && std::atof (durBefore.c_str()) > 2.5
               && std::fabs (std::atof (durBefore.c_str()) - std::atof (durAfter.c_str())) < 0.05,
               "długość dźwięku bez zmian", durBefore + " s / " + durAfter + " s");
        // ponowny zapis nie dubluje tagu
        writeAll (tagged, withTag (readAll (tagged), t));
        check (readAll (tagged).size() == raw.size() - oldTag + tag.size(), "drugi zapis nie dubluje tagu");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
