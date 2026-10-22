/*
    zs-tekst — co padlo w nagraniu, a co mialo pasc (DSP/ScriptAlign.h).

    Wejscie: slowa z czasami z whisper.cpp i tekst promptera.

      whisper-cli -m ggml-small.bin -l pl -f nagranie16k.wav -ml 1 -sow -ocsv -of slowa
      zs-tekst slowa.csv tekst.txt

    Wynik: powtorzenia (wczesniejsze podejscia do wyciecia), slowa spoza
    tekstu, przejezyczenia w ostatnim podejsciu, pominiete fragmenty.

    Budowanie: g++ -std=c++17 -O2 Tools/zs-tekst.cpp -o zs-tekst
*/

#include "../Source/DSP/ScriptAlign.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string readAll (const char* path)
{
    std::ifstream in (path, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

/** CSV z whisper-cli -ocsv: start,end,"tekst" (czasy w ms). */
static std::vector<tw::HeardWord> readWhisperCsv (const std::string& csv)
{
    std::vector<tw::HeardWord> out;
    std::istringstream in (csv);
    std::string line;
    std::getline (in, line);                                     // naglowek
    while (std::getline (in, line))
    {
        const size_t a = line.find (','), b = line.find (',', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        std::string text = line.substr (b + 1);
        if (! text.empty() && text.front() == '"') text = text.substr (1);
        if (! text.empty() && text.back() == '\r') text.pop_back();
        if (! text.empty() && text.back() == '"') text.pop_back();
        while (! text.empty() && text.front() == ' ') text.erase (0, 1);
        tw::HeardWord w;
        w.from = std::atof (line.substr (0, a).c_str()) / 1000.0;
        w.to   = std::atof (line.substr (a + 1, b - a - 1).c_str()) / 1000.0;
        w.text = text;
        if (! text.empty()) out.push_back (w);
    }
    return out;
}

static std::string clock (double s)
{
    char b[32]; std::snprintf (b, sizeof (b), "%d:%04.1f", (int) s / 60, s - 60 * (int) (s / 60));
    return b;
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uzycie: zs-tekst slowa.csv tekst.txt\n"); return 2; }
    const auto heard = readWhisperCsv (readAll (argv[1]));
    const auto script = readAll (argv[2]);
    const auto found = tw::compareWithScript (heard, script);

    int counts[4] = {};
    for (const auto& f : found)
    {
        ++counts[(int) f.kind];
        switch (f.kind)
        {
            case tw::ScriptFinding::Kind::Repeat:
                std::printf ("%s–%s  POWTÓRZENIE     %s\n", clock (f.from).c_str(), clock (f.to).c_str(), f.heard.c_str()); break;
            case tw::ScriptFinding::Kind::OffScript:
                std::printf ("%s–%s  POZA TEKSTEM    %s\n", clock (f.from).c_str(), clock (f.to).c_str(), f.heard.c_str()); break;
            case tw::ScriptFinding::Kind::Slip:
                std::printf ("%s–%s  PRZEJĘZYCZENIE  „%s” zamiast „%s”\n", clock (f.from).c_str(), clock (f.to).c_str(), f.heard.c_str(), f.expected.c_str()); break;
            case tw::ScriptFinding::Kind::Skipped:
                std::printf ("%s         POMINIĘTE       %s\n", clock (f.from).c_str(), f.expected.c_str()); break;
        }
    }
    std::printf ("\nslow w zapisie: %zu, powtorzen: %d, poza tekstem: %d, przejezyczen: %d, pominietych fragmentow: %d\n",
                 heard.size(), counts[0], counts[1], counts[2], counts[3]);
    return 0;
}
