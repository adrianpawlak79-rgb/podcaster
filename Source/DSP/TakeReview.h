/*
    TakeReview.h — co sie dzieje po zatrzymaniu nagrania.

    ---------------------------------------------------------------------------
    NA JAKIE PYTANIE TO ODPOWIADA

    Czlowiek wlasnie przestal mowic. Ma w glowie dokladnie jedno pytanie:

        czy to nadaje sie do uzycia, czy trzeba powtorzyc — i jesli tak, to co.

    Nie "jaki jest LUFS", nie "ile wynosi redukcja". Te liczby sa potrzebne,
    ale jako UZASADNIENIE odpowiedzi, a nie zamiast niej. Program, ktory po
    nagraniu pokazuje tabelke pomiarow, zrzuca na czlowieka prace, ktora sam
    mial wykonac.

    ---------------------------------------------------------------------------
    DWIE ROZNE RZECZY, KTORE LATWO POMYLIC

    Zdarzenia z lampki dziela sie na dwie grupy i to jest NAJWAZNIEJSZY podzial
    na tym ekranie:

        DO NAPRAWY   — sybilanty, plozywy, mlaski, oddechy, przydzwiek, szum,
                       odjezdzanie poziomu. Miks sie tym zajmuje i po miksie
                       tego nie ma. Nie ma o czym mowic czlowiekowi, poza
                       informacja, ze zostalo zrobione.

        DO POWTORZENIA — przester i cisza cyfrowa. Probki sa zniszczone albo ich
                       nie ma. Zaden miks tego nie przywroci; da sie tylko
                       nagrac ten fragment jeszcze raz.

    Program dotad wrzucal jedno i drugie na ta sama liste, w tej samej szarosci.
    Przez to najwazniejsza wiadomosc — "wers trzeci trzeba powtorzyc" — lezala
    miedzy czternastoma wpisami o oddechach.

    ---------------------------------------------------------------------------
    ZGRUPOWANE, NIE POJEDYNCZE

    Minuta prawdziwego mowienia daje kilkadziesiat zdarzen, w tym serie po
    kilka tego samego w ciagu dwoch sekund. Jako lista to jest nieczytelne, a
    jako kreski na osi czasu — las. Wiec zdarzenia tego samego rodzaju, lezace
    blisko siebie, skladaja sie w jeden kawalek osi z liczba powtorzen.

    ---------------------------------------------------------------------------
    CZEGO TU NIE MA

    Zadnego suwaka. Ekran po nagraniu nie jest miejscem na ustawianie miksu:
    czlowiek wlasnie przestal mowic, nie ma uwagi na dwadziescia parametrow, a
    program ma wszystko, czego potrzeba, zeby wybrac sam (Decide.h) — i umie
    powiedziec jednym zdaniem, dlaczego tak wybral.

    Jest tylko przelacznik PRZED/PO, bo to jedyne pytanie, na ktore liczby nie
    odpowiadaja, a ucho odpowiada w sekunde.
*/

#pragma once

#include "TakeWatchCore.h"
#include "Mix/Decide.h"
#include "Mix/SpeechChain.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include "RealtimeSlot.h"
#include <memory>
#include <string>
#include <vector>

namespace zs
{

//==============================================================================
/** Czy tego rodzaju zdarzenie da sie jeszcze naprawic po nagraniu.

    To ta sama wiedza, ktora w MainComponent decyduje, czy wolno uciszyc alarm
    przyciskiem "To bylo OK" — i celowo stoi tutaj, w naglowku bez JUCE, zeby
    oba ekrany braly ja z jednego miejsca, a nie kazdy po swojemu. */
inline bool fixableInMix (tw::Kind k)
{
    return k != tw::Kind::Clipping && k != tw::Kind::DigitalSilence;
}

//==============================================================================
/** Kawalek osi czasu: jedno zdarzenie albo seria tego samego rodzaju. */
struct Trouble
{
    tw::Kind kind     = tw::Kind::Clipping;
    double   fromSec  = 0.0;
    double   toSec    = 0.0;
    double   redoFrom = 0.0;   ///< poczatek frazy, do ktorej nalezy
    float    worst    = 0.0f;  ///< najgorsza zmierzona liczba w serii
    int      count    = 1;
    int      severity = 0;
    bool     fixable  = true;

    /** Czy to miejsce trafia na liste "nagraj jeszcze raz".

        To NIE jest to samo co "nie do naprawienia". Cisza cyfrowa jest nie do
        naprawienia zawsze, ale powodem do powrotu do mikrofonu tylko wtedy, gdy
        jest dziura w normalnym nagraniu — a nie wtedy, gdy cale nagranie takie
        jest. Patrz TakeReview::silenceIsTheMaterial(). */
    bool     mustRedo = false;
};

//==============================================================================
/** Fragment do powtorzenia. Powstaje tylko z rzeczy, ktorych miks nie naprawi. */
struct Redo
{
    double fromSec = 0.0;
    double toSec   = 0.0;
    tw::Kind kind  = tw::Kind::Clipping;
    int    count   = 1;
};

//==============================================================================
/** Odsluch PRZED i PO, przelaczany w trakcie grania.

    ---------------------------------------------------------------------------
    DLACZEGO POZYCJA JEST WSPOLNA

    Przelaczenie, ktore zaczyna od nowa, nie odpowiada na zadne pytanie: zanim
    dojdziesz do spornego miejsca, nie pamietasz juz, jak brzmialo. Porownanie
    ma sens tylko wtedy, gdy oba przebiegi ida pod ta sama igla — wiec pozycja
    jest jedna i przelacznik zmienia WYLACZNIE to, z ktorego bufora czytamy.

    ---------------------------------------------------------------------------
    DLACZEGO TO JEST TAKIE OSTROZNE

    Czytanie dzieje sie w watku dzwieku, ktory nie ma prawa czekac na nic:
    zadnego przydzielania pamieci, zadnego blokowania sie na muteksie
    trzymanym przez interfejs. Dlatego bufory podmienia sie TYLKO wtedy, gdy
    nic nie gra, a watek dzwieku dotyka wylacznie wskaznikow i liczb atomowych.
*/
class Audition
{
public:
    // setMaterial/clear/destruction belong to the control thread.
    ~Audition() { clear(); }
    void setMaterial(std::vector<float> before, std::vector<float> after, double sampleRate,
                     bool matchLoudness = false)
    {
        stop(); slot.set(nullptr);
        auto next = std::make_unique<Material>();
        next->a = std::move(before);next->b = std::move(after);
        next->smooth = matchLoudness;
        if(matchLoudness && !next->a.empty() && !next->b.empty()) {
            auto lufs = [sampleRate](const std::vector<float>& x) {
                mix::LoudnessMeter m;m.prepare(sampleRate,1);
                const float* p[]={x.data()};m.process(p,1,(int)x.size());return m.integrated();
            };
            double la=lufs(next->a),lb=lufs(next->b);
            next->ga=(float)std::pow(10.,std::min(0.,lb-la)/20.);
            next->gb=(float)std::pow(10.,std::min(0.,la-lb)/20.);
        }
        owned=std::move(next);
        sr.store(sampleRate);len.store((int)std::min(owned->a.size(),owned->b.size()));
        pos.store(0);seek.store(0);cursor=0;
        blend=wantAfter.load()?1.f:0.f;
        slot.set(owned.get());
    }
    void clear() {
        stop();slot.set(nullptr);owned.reset();len.store(0);pos.store(0);seek.store(0);
    }
    void play() { if(len.load()>0) playing.store(true); }
    void pause() { playing.store(false); }
    void stop() { playing.store(false);seek.store(0); }
    bool isPlaying() const noexcept { return playing.load(); }
    bool hasMaterial() const noexcept { return len.load()>0; }
    void showAfter(bool after) noexcept { wantAfter.store(after); }
    bool showingAfter() const noexcept { return wantAfter.load(); }
    void toggle() noexcept { wantAfter.store(!wantAfter.load()); }
    double lengthSec() const noexcept { return sr.load()>0 ? len.load()/sr.load() : 0; }
    double positionSec() const noexcept {
        const int requested=seek.load();
        return sr.load()>0 ? (requested>=0?requested:pos.load())/sr.load() : 0;
    }
    void seekSec(double t) {
        if(len.load()>0 && sr.load()>0)
            seek.store(std::clamp((int)std::llround(t*sr.load()),0,len.load()-1));
    }
    bool pull(float* dest,int numSamples) noexcept {
        RealtimeSlot<Material>::Read read(slot);
        if(!read)return false;
        const int requested=seek.exchange(-1);
        if(requested>=0) { cursor=requested;pos.store(cursor); }
        if(!playing.load())return false;
        const int n=(int)std::min(read->a.size(),read->b.size());
        const float goal=wantAfter.load()?1.f:0.f;
        const float step=(float)(1.0/std::max(1.0,sr.load()*.005));
        for(int i=0;i<numSamples;++i) {
            if(cursor>=n) {
                std::fill(dest+i,dest+numSamples,0.f);
                playing.store(false);pos.store(std::max(0,n-1));return true;
            }
            blend=read->smooth ? blend+std::clamp(goal-blend,-step,step) : goal;
            dest[i]=read->a[(size_t)cursor]*read->ga*(1-blend)
                  + read->b[(size_t)cursor]*read->gb*blend;
            ++cursor;
        }
        // Does not overwrite a pending seek/stop command.
        pos.store(cursor);return true;
    }
private:
    struct Material { std::vector<float>a,b;float ga=1,gb=1;bool smooth=false; };
    std::unique_ptr<Material> owned;
    RealtimeSlot<Material> slot;
    std::atomic<double> sr{0};
    std::atomic<int>len{0},pos{0},seek{0};
    std::atomic<bool>playing{false},wantAfter{true};
    int cursor=0;float blend=1;
};

//==============================================================================
/** Przebieg nagrania do narysowania: szczyt na kolumne, w decybelach. */
struct Envelope
{
    std::vector<float> peakDb;      ///< tyle wartosci, ile kolumn
    double seconds = 0.0;

    void build (const std::vector<float>& x, double sr, int columns)
    {
        build (x, sr, columns, 0, x.size());
    }

    /** Tylko fragment [fromSec, toSec) — powiekszona fala w montazu. Bez
        kopii probek: godzina to 0,7 GB, a fragment liczy sie przy kazdym
        powiekszeniu. */
    void buildRange (const std::vector<float>& x, double sr, int columns, double fromSec, double toSec)
    {
        if (sr <= 0.0) { build (x, sr, columns, 0, 0); return; }
        const size_t a = (size_t) std::clamp (fromSec * sr, 0.0, (double) x.size());
        const size_t b = (size_t) std::clamp (toSec   * sr, (double) a, (double) x.size());
        build (x, sr, columns, a, b);
    }

    void build (const std::vector<float>& x, double sr, int columns, size_t first, size_t last)
    {
        peakDb.assign ((size_t) std::max (1, columns), -140.0f);
        seconds = sr > 0.0 ? (last - first) / sr : 0.0;

        if (last <= first || columns <= 0) return;

        const size_t n = last - first;
        for (int c = 0; c < columns; ++c)
        {
            const size_t from = first + (size_t) ((double) c       / columns * n);
            const size_t to   = first + (size_t) ((double) (c + 1) / columns * n);

            float peak = 0.0f;

            for (size_t i = from; i < to && i < last; ++i)
                peak = std::max (peak, std::fabs (x[i]));

            peakDb[(size_t) c] = peak > 1.0e-7f ? 20.0f * std::log10 (peak) : -140.0f;
        }
    }
};

//==============================================================================
class TakeReview
{
public:
    //==========================================================================
    /** Co jest do zrobienia. Miks trwa sekundy, wiec ekran musi umiec
        powiedziec, na czym stoi — inaczej wyglada na zawieszony. */
    enum class Stage { Empty, Listening, Mixing, Ready, Failed };

    /* st i prog czyta interfejs, a pisze watek roboczy — stad atomy. Bez nich
       ekran czytalby wartosc w trakcie zapisu, a to jest ten rodzaj bledu,
       ktory pojawia sie raz na kilkadziesiat nagran i nigdy nie da sie go
       powtorzyc na zyczenie. */
    Stage stage() const noexcept { return st.load(); }

    /** 0..1. Zgrubnie, bo dokladny postep wymagalby przerobienia lancucha —
        a lepsze zgrubnie uczciwe niz dokladne udawane. */
    double progress() const noexcept { return prog.load(); }

    //==========================================================================
    /** Material do oceny. Zdarzenia sa te same, ktore widzialo oko w trakcie
        nagrywania — nie liczymy ich jeszcze raz, bo dwie odpowiedzi na to samo
        pytanie zawsze kiedys sie rozjada. */
    /** Przesluchanie nagrania, ktore nie bylo sledzone na zywo.

        Nagranie otwarte z dysku — wczorajsze, cudze, odzyskane — nie ma listy
        zdarzen, bo nikt nie patrzyl na lampke, kiedy powstawalo. Bez tego ekran
        po nagraniu powiedzialby o kazdym takim pliku "nadaje sie w calosci",
        takze o takim, ktory jest przesterowany od poczatku do konca. Milcząca
        zgoda na zniszczony material jest gorsza niz brak ekranu.

        Wiec przepuszczamy go przez dokladnie ten sam TakeWatch, ktory patrzy w
        trakcie mowienia. Ten sam, a nie drugi podobny: dwa osobne wykrywacze
        tego samego predzej czy pozniej zaczynaja mowic rozne rzeczy o tym samym
        nagraniu, a wtedy nie wiadomo, ktoremu wierzyc. */
    static std::vector<tw::Event> listenOffline (const std::vector<float>& x, double sr)
    {
        tw::TakeWatch watch;
        watch.prepare (sr, 512);
        watch.reset();

        std::vector<tw::Event> events;

        for (size_t pos = 0; pos < x.size(); pos += 512)
        {
            const int n = (int) std::min ((size_t) 512, x.size() - pos);
            const float* ch[1] = { x.data() + pos };

            watch.process (ch, 1, n, (double) pos / sr);

            tw::Event e;
            while (watch.nextEvent (e)) events.push_back (e);
        }

        return events;
    }

    //==========================================================================
    void setTake (std::vector<float> raw, double sampleRate, std::vector<tw::Event> events)
    {
        before = std::move (raw);
        sr     = sampleRate;
        marks  = std::move (events);

        mixed.clear();
        lines.clear();
        troubles.clear();
        redos.clear();

        st.store (before.empty() ? Stage::Empty : Stage::Listening);
        prog.store (0.0);

        silentPart = digitalSilenceRatio (before, sr);

        groupTroubles();
    }

    //==========================================================================
    /** Cala robota. Wolane z watku roboczego, nie z interfejsu — na minucie
        mowy lancuch chodzi kilka sekund, a zamrozone okno wyglada na awarie. */
    /** Czego program chce od miksu, gdy czlowiek nic nie wybral: podcast,
        -16 LUFS.

        Dotad byl tu tryb Auto ("tam, gdzie nie trzeba ruszac szczytu, ale nie
        ciszej niz -23"), a ekran oceny nie mial wyboru celu. Symulacja sesji
        (Docs/badania/SYMULACJA.md) pokazala skutek: jeden szczyt — przester
        albo plozja — sciszal CALY odcinek do -23 LUFS, o 7 dB ciszej niz
        podcasty w Apple i Spotify. Pojedyncze szczyty ma zatrzymac limiter,
        nie glosnosc calosci. */
    static mix::Decide::Wish podcastWish()
    {
        mix::Decide::Wish w;
        w.where = mix::Decide::Destination::Podcast;
        return w;
    }

    void run (mix::Decide::Wish wish = podcastWish())
    {
        if (before.empty()) { st.store (Stage::Empty); return; }

        st.store (Stage::Listening);
        prog.store (0.05);

        decision = mix::Decide::listen (before, sr, wish);

        prog.store (0.35);
        st.store (Stage::Mixing);

        mix::SpeechChain chain;
        chain.params = decision.params;
        chain.setSampleRate (sr);
        chain.prepare (sr);

        report = chain.render (before, mixed);

        prog.store (0.95);

        buildLines();

        st.store (Stage::Ready);
        prog.store (1.0);
    }

    //==========================================================================
    const std::vector<float>& rawAudio()   const noexcept { return before; }
    const std::vector<float>& mixedAudio() const noexcept { return mixed; }
    double sampleRate() const noexcept { return sr; }

    double seconds() const noexcept { return sr > 0.0 ? before.size() / sr : 0.0; }

    const std::vector<Trouble>& timeline() const noexcept { return troubles; }
    const std::vector<Redo>&    toRedo()   const noexcept { return redos; }

    const mix::Decision&   whatIDecided() const noexcept { return decision; }
    const mix::ChainReport& whatIDid()    const noexcept { return report; }

    /** Zdania dla czlowieka, w kolejnosci waznosci. */
    const std::vector<std::string>& says() const noexcept { return lines; }

    //==========================================================================
    /** Jedno zdanie na gorze ekranu: werdykt.

        0 = nadaje sie, 1 = nadaje sie, ale cos warto powtorzyc,
        2 = sa fragmenty, ktorych nie da sie naprawic. */
    bool mixValid() const noexcept {
        return std::isfinite(report.peakAfter) && std::isfinite(report.lufsAfter)
            && report.peakAfter <= decision.params.ceilingDbTp + 0.1;
    }
    int verdict() const noexcept
    {
        if (st.load() == Stage::Ready && !mixValid()) return 2;
        if (! redos.empty())
            return 2;

        for (const auto& t : troubles)
            if (t.severity >= (int) tw::Severity::Redo)
                return 1;

        return 0;
    }

    std::string verdictLine() const
    {
        char b[320];

        if(st.load()==Stage::Ready && !mixValid())
            return "Miks nie spełnia ograniczenia szczytu. Nie publikuj tej wersji.";
        switch (verdict())
        {
            case 2:
                if (redos.size() == 1)
                    std::snprintf (b, sizeof (b),
                        "Jeden fragment trzeba nagrać jeszcze raz: %s. Tego miks nie cofa.",
                        timeRange (redos[0].fromSec, redos[0].toSec).c_str());
                else
                    std::snprintf (b, sizeof (b),
                        "Fragmentów do powtórzenia: %d. Wykryłem przester albo brak sygnału.", (int) redos.size());

                return b;

            case 1:
            {
                // Dawniej: "Odsluchaj zaznaczone miejsca" — bez liczby i bez
                // czasu, a panel obok mowil "nie ma fragmentow do powtorzenia"
                // (to inna lista: rzeczy, ktorych miks NIE naprawi). Czlowiek
                // widzial dwa zdania, ktore sobie przecza.
                int n = 0;
                double first = -1.0;

                for (const auto& t : troubles)
                    if (t.severity >= (int) tw::Severity::Redo)
                    {
                        ++n;
                        if (first < 0.0 || t.fromSec < first) first = t.fromSec;
                    }

                if (n == 1)
                    std::snprintf (b, sizeof (b),
                        "Miks jest gotowy. Posłuchaj jednego miejsca na osi (%s) — "
                        "nie trzeba go nagrywać od nowa.", atTime (first).c_str());
                else
                    std::snprintf (b, sizeof (b),
                        "Miks jest gotowy. Posłuchaj %d miejsc na osi (pierwsze %s) — "
                        "nie trzeba ich nagrywać od nowa.", n, atTime (first).c_str());
                return b;
            }

            default:
                return "Nie wykryłem przesteru ani braków sygnału wymagających powtórzenia.";
        }
    }

    //==========================================================================
    /** Czas w formacie, ktory czlowiek od razu znajdzie na osi. */
    static std::string atTime (double s)
    {
        char b[32];
        const int t = (int) s;
        std::snprintf (b, sizeof (b), "%d:%02d", t / 60, t % 60);
        return b;
    }

    static std::string timeRange (double from, double to)
    {
        return atTime (from) + "–" + atTime (std::max (to, from + 1.0));
    }

private:
    //==========================================================================
    /* Skladanie zdarzen w kawalki osi.

       Dwa zdarzenia tego samego rodzaju blizej niz 2,5 s to jedno miejsce w
       nagraniu, nie dwa — tyle wynosi dlugosc frazy, w ktorej czlowiek i tak
       nie rozdzieli ich sluchem. Rozne rodzaje nigdy sie nie skladaja, bo
       "przester" i "oddech" w tej samej sekundzie to dwie zupelnie rozne
       wiadomosci. */
    void groupTroubles()
    {
        troubles.clear();

        auto byTime = marks;
        std::stable_sort (byTime.begin(), byTime.end(),
                          [] (const tw::Event& x, const tw::Event& y) { return x.timeSec < y.timeSec; });

        constexpr double joinSec = 2.5;

        for (const auto& e : byTime)
        {
            Trouble* into = nullptr;

            // Szukamy od konca: seria, do ktorej to nalezy, jest najswiezsza.
            for (size_t i = troubles.size(); i-- > 0;)
            {
                if (troubles[i].kind == e.kind && e.timeSec - troubles[i].toSec <= joinSec)
                {
                    into = &troubles[i];
                    break;
                }

                if (troubles[i].toSec < e.timeSec - joinSec)
                    break;
            }

            if (into != nullptr)
            {
                into->toSec    = e.timeSec;
                into->count   += 1;
                into->worst    = std::fabs (e.value) > std::fabs (into->worst) ? e.value : into->worst;
                into->severity = std::max (into->severity, (int) e.severity);
                continue;
            }

            Trouble t;
            t.kind     = e.kind;
            t.fromSec  = e.timeSec;
            t.toSec    = e.timeSec;
            t.redoFrom = e.redoFrom > 0.0 ? e.redoFrom : e.timeSec;
            t.worst    = e.value;
            t.severity = (int) e.severity;
            t.fixable  = fixableInMix (e.kind);

            troubles.push_back (t);
        }

        buildRedos();
    }

    /* Lista do powtorzenia — wylacznie z rzeczy, ktorych miks nie naprawi.

       Fragment liczy sie od POCZATKU FRAZY (redoFrom), nie od chwili zdarzenia.
       Przester slychac w jednej sylabie, ale nagrac trzeba cale zdanie, bo
       wklejenie polowy slowa nie da sie zrobic tak, zeby nie bylo slychac. */
    /* Jaka czesc nagrania to cisza ZUPELNA — probki na zerze, nie cichy pokoj.

       Prawdziwy mikrofon zawsze cos daje: wentylator, ulica, szum przedwzmacniacza.
       Cisza ponizej -90 dBFS znaczy, ze sygnalu NIE MA — albo wysiadl kabel, albo
       material zostal wczesniej zbramkowany, albo nie pochodzi z mikrofonu.

       Rozroznienie jest potrzebne dokladnie po to, co nizej. */
    static double digitalSilenceRatio (const std::vector<float>& x, double sr)
    {
        if (x.empty() || sr <= 0.0) return 0.0;

        const size_t block = (size_t) std::max (1.0, sr * 0.05);
        size_t silent = 0, total = 0;

        for (size_t pos = 0; pos < x.size(); pos += block)
        {
            float peak = 0.0f;

            for (size_t i = pos; i < pos + block && i < x.size(); ++i)
                peak = std::max (peak, std::fabs (x[i]));

            ++total;
            if (peak < 3.2e-5f) ++silent;     // -90 dBFS
        }

        return total > 0 ? (double) silent / (double) total : 0.0;
    }

    /* Lista do powtorzenia — ciag dalszy.

       Cisza cyfrowa jest w regule "nie do naprawienia", bo w trakcie nagrywania
       znaczy jedno: sygnal zniknal, sprawdz kabel. Ale ekran po nagraniu patrzy
       na CALY material naraz i widzi wiecej.

       Material, ktory jest cisza cyfrowa w co dziesiatej chwili, nie ma urwanego
       kabla — on po prostu caly taki jest: zbramkowany w innym programie,
       wygenerowany albo zlozony z kawalkow. Wpisanie wtedy dziewieciu "powtorz
       0:03-0:04" to odeslanie czlowieka do mikrofonu po nic, a to jest ten
       rodzaj bledu, po ktorym przestaje sie czytac cala liste — takze te
       pozycje, ktore sa prawdziwe.

       Wiec: pojedyncza dziura w normalnym nagraniu zostaje powodem do
       powtorzenia. Cisza, ktora jest charakterem calego pliku, trafia do zdan,
       a nie na liste powtorek. */
    bool silenceIsTheMaterial() const noexcept { return silentPart > 0.10; }

    void buildRedos()
    {
        redos.clear();

        for (auto& t : troubles)
        {
            t.mustRedo = false;

            if (t.fixable) continue;

            if (t.kind == tw::Kind::DigitalSilence && silenceIsTheMaterial())
                continue;

            t.mustRedo = true;

            const double from = t.redoFrom;
            const double to   = std::max (t.toSec, from + 1.0);

            if (! redos.empty() && from <= redos.back().toSec + 1.0)
            {
                redos.back().toSec  = std::max (redos.back().toSec, to);
                redos.back().count += t.count;
                continue;
            }

            Redo r;
            r.fromSec = from;
            r.toSec   = to;
            r.kind    = t.kind;
            r.count   = t.count;
            redos.push_back (r);
        }
    }

    //==========================================================================
    /* Zdania o tym, co program zrobil.

       Uzasadnienia z Decide.h mowia, CO program postanowil i z jakiej liczby.
       Tutaj doklada sie to, co wyszlo PO zrobieniu — bo postanowienie i skutek
       to nie to samo, a jedyny sposob, zeby ktos nam kiedykolwiek zaufal, to
       pokazywac oba. */
    void buildLines()
    {
        lines.clear();
        char b[400];

        std::snprintf (b, sizeof (b),
            "Wynik: było %.1f LUFS, jest %.1f LUFS. Szczyt %.1f dBTP przy suficie %.1f dBTP.",
            report.lufsBefore, report.lufsAfter, report.peakAfter, (double) decision.params.ceilingDbTp);
        lines.push_back (b);

        // Sufit jest DEKLARACJA, nie zyczeniem. Jesli go przekroczylismy, to
        // musi stac na ekranie, a nie w komentarzu w kodzie — plik, ktory
        // trzeszczy po konwersji do mp3, i tak sie wyda, tylko pozniej i u
        // kogos innego.
        if (report.peakAfter > decision.params.ceilingDbTp + 0.1)
        {
            std::snprintf (b, sizeof (b),
                "UWAGA: szczyt wyszedł %.1f dB nad zadeklarowany sufit. To błąd w miksie, "
                "nie w nagraniu — zgłoś go.",
                report.peakAfter - decision.params.ceilingDbTp);
            lines.push_back (b);
        }

        for (const auto& why : decision.because)
            lines.push_back (why);

        if (report.clicksRepaired > 0)
        {
            std::snprintf (b, sizeof (b), "Naprawione trzaski: %d.", report.clicksRepaired);
            lines.push_back (b);
        }

        if (report.denoiseDb < 0.0)
        {
            std::snprintf (b, sizeof (b),
                "Odszumianie włączone, dno %.0f dB — tło pokoju leżało %.0f dB pod mową, "
                "czyli dość blisko, żeby było słyszalne w pauzach.",
                report.denoiseDb, report.speechDb - report.noiseFloorDb);
            lines.push_back (b);
        }

        if (report.tiltUsed)
        {
            std::snprintf (b, sizeof (b),
                "Cofnięcie barwy po kompresji: %+.1f dB na 300 Hz, %+.1f na 500, "
                "%+.1f na 1,4 k, %+.1f na 5 k. To naprawa skutku ubocznego, nie efekt.",
                (double) report.tiltDb[0], (double) report.tiltDb[1],
                (double) report.tiltDb[2], (double) report.tiltDb[3]);
            lines.push_back (b);
        }

        if (silenceIsTheMaterial())
        {
            std::snprintf (b, sizeof (b),
                "Przez %.0f%% czasu w pliku jest cisza cyfrowa. Może pochodzić z bramkowania "
                "lub wyciszenia. Sprawdź, czy w tych przerwach nie zginęły słowa.", silentPart * 100.0);
            lines.push_back (b);
        }

        int fixed = 0;
        for (const auto& t : troubles) if (t.fixable) fixed += t.count;

        if (fixed > 0)
        {
            std::snprintf (b, sizeof (b),
                "Wykrytych zdarzeń do sprawdzenia w odsłuchu: %d. "
                "Włączenie korekty nie potwierdza usunięcia każdego z nich.", fixed);
            lines.push_back (b);
        }
    }

    //==========================================================================
    std::vector<float> before, mixed;
    double sr = 48000.0;

    std::vector<tw::Event> marks;
    std::vector<Trouble>   troubles;
    std::vector<Redo>      redos;

    mix::Decision    decision;
    mix::ChainReport report;

    std::vector<std::string> lines;

    double silentPart = 0.0;

    std::atomic<Stage>  st   { Stage::Empty };
    std::atomic<double> prog { 0.0 };
};

} // namespace zs
