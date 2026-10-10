/*
    zs-porownaj — przygotowuje odsłuch porównawczy z jednego nagrania.

        zs-porownaj wejscie.wav folder-wyjsciowy

    Robi kilka wersji tego samego materialu i stronę, na ktorej da sie miedzy
    nimi przelaczac W TRAKCIE grania, bez przerwy i bez przeskoku pozycji.

    ---------------------------------------------------------------------------
    DLACZEGO WSZYSTKIE WERSJE MAJA TE SAMA GLOSNOSC

    To jest jedyny powod, dla ktorego to narzedzie w ogole istnieje.

    Ucho myli glosnosc z jakoscia. Roznica jednego decybela wystarczy, zeby
    glosniejsza wersja wydala sie "pelniejsza", "cieplejsza" i "lepiej
    zmiksowana" — i nie da sie tego wysilkiem woli wylaczyc, bo to nie jest
    kwestia skupienia, tylko budowy sluchu.

    Nasz lancuch podnosi material do -16 LUFS. Oryginal jest zwykle
    kilkanascie decybeli cichszy. Porownanie "przed i po" wprost jest wiec
    ustawione tak, ze wynik jest znany z gory: obrobione WYGRA, nawet gdyby
    lancuch psul dzwiek. Test, ktory nie moze przegrac, nie jest testem.

    Dlatego kazda wersja, lacznie z oryginalem, jest tu sprowadzona do tej
    samej zmierzonej glosnosci. Wtedy porownujesz BARWE i to, jak rzecz
    oddycha — czyli to, o co naprawde chodzi.

    ---------------------------------------------------------------------------
    DLACZEGO KILKA WERSJI, A NIE JEDNA

    "Brzmi zle" nie jest informacja, z ktora da sie cos zrobic. "Brzmi zle, a
    bez de-essera juz nie" — jest. Kazdy wariant ma wylaczony dokladnie jeden
    stopien, wiec jesli ktorys z nich brzmi lepiej od pelnego, wiadomo od razu,
    ktory modul jest do poprawki.
*/

#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

//==============================================================================
struct Variant
{
    const char* file;
    const char* name;
    const char* what;      ///< czego sluchac
};

static double measureLufs (const std::vector<float>& v, double sr)
{
    mix::LoudnessMeter m;
    m.prepare (sr, 1);
    const float* p[1] = { v.data() };
    m.process (p, 1, (int) v.size());
    return m.integrated();
}

//==============================================================================
static void writePage (const std::string& dir, const std::vector<Variant>& vs,
                       const std::string& source, double seconds)
{
    std::string h;
    h.reserve (16000);

    h += "<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\">";
    h += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    h += "<title>Odsluch porownawczy</title><style>";
    h += ":root{--bg:#0f1412;--panel:#18201d;--edge:#2a3733;--ink:#e9efec;--ink2:#93a49b;"
         "--green:#35b98d;--amber:#d9a641;"
         "--mono:ui-monospace,'SF Mono',Consolas,monospace;"
         "--sans:system-ui,-apple-system,'Segoe UI',sans-serif}"
         "*{box-sizing:border-box}"
         "body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--sans);"
         "font-size:15px;line-height:1.6}"
         ".wrap{max-width:760px;margin:0 auto;padding:34px 20px 70px}"
         "h1{font-size:21px;margin:0 0 4px;font-weight:600}"
         ".sub{color:var(--ink2);font-family:var(--mono);font-size:12.5px;margin:0 0 26px}"
         ".note{border-left:3px solid var(--amber);background:#221d12;padding:13px 16px;"
         "margin:0 0 26px;font-size:14px;color:#e8dcc2}"
         ".note b{color:#fff}"
         ".transport{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:8px}"
         "button{font-family:var(--mono);font-size:13px;background:var(--panel);color:var(--ink);"
         "border:1px solid var(--edge);border-radius:4px;padding:9px 15px;cursor:pointer}"
         "button:hover{border-color:var(--ink2)}"
         "button:focus-visible{outline:2px solid var(--green);outline-offset:2px}"
         "#play{background:var(--green);border-color:var(--green);color:#08110d;font-weight:600;"
         "min-width:118px}"
         ".rail{height:6px;background:#0a0f0d;border:1px solid var(--edge);border-radius:3px;"
         "margin:14px 0 4px;position:relative;cursor:pointer}"
         ".rail i{position:absolute;inset:0 auto 0 0;background:var(--green);border-radius:3px;width:0}"
         ".time{font-family:var(--mono);font-size:12px;color:var(--ink2);"
         "display:flex;justify-content:space-between;font-variant-numeric:tabular-nums}"
         "ul{list-style:none;padding:0;margin:26px 0 0;display:grid;gap:8px}"
         "li{display:grid;grid-template-columns:auto 1fr;gap:14px;align-items:start;"
         "border:1px solid var(--edge);border-radius:4px;padding:12px 14px;background:var(--panel);"
         "cursor:pointer}"
         "li.on{border-color:var(--green);background:#16241f}"
         "li .key{font-family:var(--mono);font-size:12px;color:var(--ink2);"
         "border:1px solid var(--edge);border-radius:3px;padding:2px 8px;margin-top:2px}"
         "li.on .key{color:var(--green);border-color:var(--green)}"
         "li b{display:block;font-weight:600;font-size:15px}"
         "li span{color:var(--ink2);font-size:13.5px}"
         "footer{margin-top:34px;color:#63736b;font-size:12.5px;border-top:1px solid var(--edge);"
         "padding-top:14px}"
         "@media(max-width:520px){li{grid-template-columns:1fr}}"
         "</style></head><body><div class=\"wrap\">";

    h += "<h1>Odsluch porownawczy</h1>";
    h += "<p class=\"sub\">" + source + " &nbsp;·&nbsp; "
       + std::to_string ((int) seconds) + " s &nbsp;·&nbsp; wszystkie wersje na -16 LUFS</p>";

    h += "<div class=\"note\"><b>Wszystkie wersje maja te sama glosnosc — lacznie z oryginalem.</b> "
         "Ucho myli glosnosc z jakoscia, wiec gdyby obrobione bylo glosniejsze, wygraloby "
         "niezaleznie od tego, czy jest lepsze. Tutaj porownujesz barwe i oddech, nie poziom.</div>";

    h += "<div class=\"transport\">"
         "<button id=\"play\" type=\"button\">Odtwarzaj</button>"
         "<button id=\"back\" type=\"button\">-5 s</button>"
         "<button id=\"fwd\" type=\"button\">+5 s</button>"
         "<span class=\"sub\" style=\"margin:0\">przelaczaj klawiszami 1-"
       + std::to_string (vs.size()) + "</span></div>";

    h += "<div class=\"rail\" id=\"rail\"><i id=\"fill\"></i></div>"
         "<div class=\"time\"><span id=\"cur\">0:00</span><span id=\"dur\">0:00</span></div>";

    h += "<ul id=\"list\">";

    for (size_t i = 0; i < vs.size(); ++i)
    {
        h += "<li data-i=\"" + std::to_string (i) + "\"" + (i == 0 ? " class=\"on\"" : "") + ">";
        h += "<span class=\"key\">" + std::to_string (i + 1) + "</span>";
        h += "<span><b>" + std::string (vs[i].name) + "</b><span>"
           + std::string (vs[i].what) + "</span></span></li>";
    }

    h += "</ul>";

    h += "<footer>Wszystkie sciezki graja rownolegle i sa zsynchronizowane; przelaczenie "
         "zmienia tylko to, ktora slychac. Dzieki temu roznice slychac natychmiast, a nie "
         "z pamieci — sluch nie utrzymuje barwy dluzej niz kilka sekund.</footer>";

    h += "</div><script>\n";

    h += "var FILES=[";
    for (size_t i = 0; i < vs.size(); ++i)
        h += (i ? "," : "") + std::string ("\"") + vs[i].file + "\"";
    h += "];\n";

    h += R"JS(
var audio = FILES.map(function(f){
  var a = new Audio(f);
  a.preload = "auto";
  a.volume = 0;
  return a;
});
var cur = 0, playing = false;

function select(i){
  if (i < 0 || i >= audio.length) return;
  cur = i;
  audio.forEach(function(a, k){ a.volume = k === i ? 1 : 0; });
  document.querySelectorAll("#list li").forEach(function(li, k){
    li.classList.toggle("on", k === i);
  });
}

function syncTo(t){ audio.forEach(function(a){ a.currentTime = t; }); }

document.getElementById("play").addEventListener("click", function(){
  if (playing){
    audio.forEach(function(a){ a.pause(); });
    this.textContent = "Odtwarzaj";
  } else {
    var t = audio[cur].currentTime;
    syncTo(t);
    audio.forEach(function(a){ a.play(); });
    this.textContent = "Zatrzymaj";
  }
  playing = !playing;
});

document.getElementById("back").addEventListener("click", function(){
  syncTo(Math.max(0, audio[cur].currentTime - 5));
});
document.getElementById("fwd").addEventListener("click", function(){
  syncTo(Math.min(audio[cur].duration || 0, audio[cur].currentTime + 5));
});

document.getElementById("rail").addEventListener("click", function(e){
  var r = this.getBoundingClientRect();
  syncTo(((e.clientX - r.left) / r.width) * (audio[cur].duration || 0));
});

document.querySelectorAll("#list li").forEach(function(li){
  li.addEventListener("click", function(){ select(+li.dataset.i); });
});

document.addEventListener("keydown", function(e){
  var n = parseInt(e.key, 10);
  if (n >= 1 && n <= audio.length) { e.preventDefault(); select(n - 1); }
  if (e.key === " ") { e.preventDefault(); document.getElementById("play").click(); }
});

function mmss(s){
  s = Math.max(0, Math.floor(s || 0));
  return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0");
}

audio[0].addEventListener("loadedmetadata", function(){
  document.getElementById("dur").textContent = mmss(audio[0].duration);
});

setInterval(function(){
  var a = audio[cur], d = a.duration || 1;
  document.getElementById("fill").style.width = (a.currentTime / d * 100) + "%";
  document.getElementById("cur").textContent = mmss(a.currentTime);
}, 120);

select(0);
)JS";

    h += "\n</script></body></html>";

    FILE* f = std::fopen ((dir + "/porownanie.html").c_str(), "wb");
    if (f) { std::fwrite (h.data(), 1, h.size(), f); std::fclose (f); }
}

//==============================================================================
int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("\nzs-porownaj — odsluch porownawczy na wyrownanej glosnosci\n\n"
                     "  zs-porownaj wejscie.wav folder-wyjsciowy\n\n"
                     "Robi szesc wersji tego samego nagrania i strone, na ktorej da sie\n"
                     "miedzy nimi przelaczac w trakcie grania. Wszystkie maja te sama\n"
                     "zmierzona glosnosc, zeby porownanie mialo sens.\n\n");
        return 1;
    }

    const std::string inPath = argv[1];
    const std::string dir = argv[2];
    const std::string mode = argc > 3 ? argv[3] : std::string();

    std::string mk = "mkdir -p \"" + dir + "\"";
#ifdef _WIN32
    mk = "if not exist \"" + dir + "\" mkdir \"" + dir + "\"";
#endif
    (void) std::system (mk.c_str());

    wavio::Audio in;
    const auto err = wavio::read (inPath, in);

    if (! err.empty()) { std::printf ("BLAD: %s\n", err.c_str()); return 2; }

    const auto mono = in.toMono();
    const double sr = (double) in.sampleRate;

    std::printf ("\n%s — %.1f s\n\n", inPath.c_str(), mono.size() / sr);

    // ---- oryginal, podniesiony do tej samej glosnosci ------------------------
    {
        const double lufs = measureLufs (mono, sr);
        const float g = mix::dbToGain ((float) (-16.0 - lufs));

        std::vector<float> lifted (mono.size());
        for (size_t i = 0; i < mono.size(); ++i)
            lifted[i] = std::max (-1.0f, std::min (1.0f, mono[i] * g));

        wavio::Audio out;
        out.sampleRate = in.sampleRate;
        out.channels.push_back (lifted);
        wavio::write16 (dir + "/0-oryginal.wav", out);

        std::printf ("  0-oryginal      %6.1f LUFS -> -16,0 (samo wzmocnienie)\n", lufs);
    }

    // ---- warianty -------------------------------------------------------------
    struct Setup { const char* file; const char* name; const char* what;
                   void (*apply) (mix::SpeechChain&); };

    // ---- tryb "deesser": sweep sily de-essera i de-clickera ----------------
    // Pierwszy odsluch na prawdziwym glosie wskazal de-esser jako podejrzanego,
    // a pomiar pokazal, ze de-clicker robi "naprawy" w ataku spiewanej nuty.
    // Ten zestaw sprawdza obie rzeczy naraz, na tym samym materiale.
    const Setup deesserSweep[] = {
        { "1-pelny-obecny.wav", "Pełny, jak jest teraz",
          "punkt odniesienia — de-esser do -12 dB, usuwanie trzasków włączone.",
          [] (mix::SpeechChain&) {} },

        { "2-bez-trzaskow.wav", "Pełny, bez usuwania trzasków",
          "jeśli TO brzmi lepiej, winne jest usuwanie trzasków, nie de-esser.",
          [] (mix::SpeechChain& c) { c.params.useDeClick = false; } },

        { "3-deesser-lagodny.wav", "De-esser łagodny",
          "tłumi najwyżej 6 dB zamiast 12, i wchodzi później.",
          [] (mix::SpeechChain& c) { c.params.essThreshDb = -8.0f; c.params.essMaxCutDb = -6.0f; } },

        { "4-lagodny-bez-trzaskow.wav", "De-esser łagodny + bez trzasków",
          "oba złagodzone naraz.",
          [] (mix::SpeechChain& c) {
              c.params.essThreshDb = -8.0f; c.params.essMaxCutDb = -6.0f;
              c.params.useDeClick = false; } },

        { "5-deesser-bardzo-lagodny.wav", "De-esser bardzo łagodny",
          "najwyżej 3 dB — ledwo dotyka.",
          [] (mix::SpeechChain& c) {
              c.params.essThreshDb = -6.0f; c.params.essMaxCutDb = -3.0f;
              c.params.useDeClick = false; } },

        { "6-bez-deessera.wav", "Bez de-essera",
          "dla odniesienia — tu sybilanty wracają w całości.",
          [] (mix::SpeechChain& c) { c.params.useDeEsser = false; c.params.useDeClick = false; } },
    };

    // ---- tryb "wiersz": mniej kompresji, bo dynamika JEST trescia -----------
    // Pierwszy odsluch na prawdziwym wierszu pokazal, ze lancuch dla podcastu
    // sciska rozrzut glosnosci z 30,8 LU do 14,8 LU. Dla mowy informacyjnej to
    // dobrze — sluchacz w samochodzie ma wszystko zrozumiec. Dla slowa
    // mowionego to jest kasowanie wykonania.
    const Setup poemSweep[] = {
        // wspolna podstawa: tryb wiersz, ktory przeszedl odsluch
        #define WIERSZ(c) c.params.useDeClick = false; \
                          c.params.riderRangeDb = 4.0f; \
                          c.params.levelerThreshDb = -18.0f; c.params.levelerRatio = 1.5f; \
                          c.params.peakThreshDb = -8.0f;     c.params.peakRatio = 2.5f;

        { "1-wczoraj.wav", "To, co wybrałeś ostatnio",
          "tryb wiersz ze starym de-esserem — punkt odniesienia.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.useTame = false;
              c.params.essThreshDb = -8.0f; c.params.essMaxCutDb = -6.0f; } },

        { "2-polka.wav", "Ten sam, ale de-esser przepisany",
          "półka zamiast dzielenia pasm — bez dziury 11 dB przy 5 kHz.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.useTame = false;
              c.params.essThreshDb = -8.0f; c.params.essMaxCutDb = -6.0f; } },

        { "3-tame.wav", "Bez de-essera, z tłumieniem odchyleń",
          "próg liczony z Twojego głosu, osobno dla każdej tercji.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.useDeEsser = false; } },

        { "4-tame-mocniej.wav", "To samo, ostrzej",
          "reaguje już na 4 dB ponad średnią, tłumi do 14.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.useDeEsser = false;
              c.params.tameThreshDb = 4.0f; c.params.tameMaxCutDb = -14.0f; } },

        { "5-tame-i-deesser.wav", "Oba naraz",
          "tłumienie odchyleń plus łagodna półka — jak u Ciebie w rack.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.essThreshDb = -8.0f; c.params.essMaxCutDb = -6.0f; } },

        { "6-tame-bez-suwaka.wav", "Tłumienie odchyleń, bez jazdy suwakiem",
          "poziom fraz zostaje taki, jaki go powiedziałeś.",
          [] (mix::SpeechChain& c) { WIERSZ(c)
              c.params.useRider = false; c.params.useDeEsser = false; } },

        #undef WIERSZ
    };

    const Setup setups[] = {
        { "1-sucho.wav", "Tylko porzadki",
          "filtr, brama i glosnosc. Bez kompresji, barwy i ciepla.",
          [] (mix::SpeechChain& c) {
              c.params.levelerRatio = 1.0f; c.params.peakRatio = 1.0f;
              c.params.useDeEsser = false;  c.params.useSaturation = false;
              c.params.useRider = false;
              c.params.mudDb = 0.0f; c.params.presenceDb = 0.0f; c.params.airDb = 0.0f; } },

        { "2-pelny.wav", "Pelny lancuch",
          "wszystko wlaczone — tak program zrobi to sam.",
          [] (mix::SpeechChain&) {} },

        { "3-bez-ciepla.wav", "Bez nasycenia",
          "jesli TO brzmi czysciej, nasycenie jest za mocne.",
          [] (mix::SpeechChain& c) { c.params.useSaturation = false; } },

        { "4-bez-deessera.wav", "Bez de-essera",
          "jesli TO brzmi naturalniej, de-esser sepleni.",
          [] (mix::SpeechChain& c) { c.params.useDeEsser = false; } },

        { "5-bez-odszumiania.wav", "Bez odszumiania",
          "jesli TO brzmi zywiej, odszumianie zjada gore glosu.",
          [] (mix::SpeechChain& c) { c.params.useDenoise = false; } },

        { "6-bez-kompresji.wav", "Bez kompresji",
          "jesli TO oddycha lepiej, kompresor pracuje za ciezko.",
          [] (mix::SpeechChain& c) {
              c.params.levelerRatio = 1.0f; c.params.peakRatio = 1.0f; } },
    };

    const Setup* table = setups;
    size_t tableSize = sizeof (setups) / sizeof (Setup);

    if (mode == "--deesser") { table = deesserSweep; tableSize = sizeof (deesserSweep) / sizeof (Setup); }
    if (mode == "--wiersz")  { table = poemSweep;    tableSize = sizeof (poemSweep) / sizeof (Setup); }

    for (size_t si = 0; si < tableSize; ++si)
    {
        const auto& s = table[si];
        mix::SpeechChain chain;
        chain.setSampleRate (sr);
        s.apply (chain);

        std::vector<float> outBuf;
        const auto rep = chain.render (mono, outBuf);

        wavio::Audio out;
        out.sampleRate = in.sampleRate;
        out.channels.push_back (outBuf);
        wavio::write16 (dir + "/" + s.file, out);

        std::printf ("  %-16s %6.1f LUFS   szczyt %5.1f dBTP\n",
                     s.file, rep.lufsAfter, rep.peakAfter);
    }

    std::vector<Variant> vs;
    vs.push_back ({ "0-oryginal.wav", "Oryginał", "surowe nagranie, tylko podgłośnione" });

    for (size_t si = 0; si < tableSize; ++si)
        vs.push_back ({ table[si].file, table[si].name, table[si].what });

    writePage (dir, vs, inPath, mono.size() / sr);

    std::printf ("\n  gotowe: %s/porownanie.html\n"
                 "  otworz w przegladarce, przelaczaj klawiszami 1-7\n\n", dir.c_str());
    return 0;
}
