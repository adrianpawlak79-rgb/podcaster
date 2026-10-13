#pragma once

#include "PunchFile.h"
#include "Look.h"
#include "DSP/TakeReview.h"

// Lista decyzji pracuje na czasach surowego pliku. Przebieg i szkic granic
// nie zmieniaja dzwieku; tylko onApply moze wywolac applyCutList.
class CutReviewPanel : public juce::Component
{
public:
    using Ink = zs::Ink;
    static juce::String tr(const char* text) { return juce::String::fromUTF8(text); }
    std::function<void(double, double)> onListen;
    std::function<void(const std::vector<tw::CutProposal>&)> onApply;
    std::function<void(const juce::String&)> onError;

    CutReviewPanel()
    {
        setWantsKeyboardFocus(true);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
        addAndMakeVisible(apply);
        apply.setButtonText(tr("Zastosuj poprawki"));
        zs::Look::setKind(apply, zs::ui::ButtonKind::Primary);
        // Zastosuj dziala od razu: niezdecydowane miejsca zostaja w nagraniu
        // (applyCutList tnie tylko zaakceptowane). Adrian 12.10: panel, ktory
        // blokowal sie do decyzji o wszystkim, byl "skomplikowany".
        apply.onClick = [this] { if (!empty() && onApply) onApply(list); };
    }

    void load(const juce::File& wav, const std::vector<float>& audio, double rate)
    {
        rows.clear(); list = wav == juce::File() ? std::vector<tw::CutProposal>{} : zs::readCutList(wav); file = wav; selected = 0;
        // Pewne najpierw; zapis zachowuje te same propozycje i ich stany.
        std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.sure > b.sure; });
        for (size_t i = 0; i < list.size(); ++i) {
            rows.push_back(std::make_unique<Row>(*this, i, audio, rate));
            body.addAndMakeVisible(*rows.back());
        }
        update(); resized();
    }
    bool empty() const { return list.empty(); }
    bool decided() const {
        return !list.empty() && std::none_of(list.begin(), list.end(), [](const auto& p) {
            return p.state == tw::CutProposal::State::Pending;
        });
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(8);
        r.removeFromTop(32);
        apply.setBounds(r.removeFromBottom(36));
        r.removeFromBottom(25);
        viewport.setBounds(r);
        const int w = juce::jmax(1, r.getWidth() - viewport.getScrollBarThickness());
        int y = 0;
        for (auto& row : rows) {
            const int h = list[row->index].sure ? 92 : 200;
            row->setBounds(0, y, w, h); y += h + 8;
        }
        body.setSize(w, y);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(Ink::panel()); g.fillRoundedRectangle(getLocalBounds().toFloat(), 6);
        int cut = 0, pending = 0;
        for (const auto& p : list) {
            if (p.state == tw::CutProposal::State::Accepted) ++cut;
            if (p.state == tw::CutProposal::State::Pending) ++pending;
        }
        g.setColour(Ink::textNow()); g.setFont(juce::Font(zs::type::body, juce::Font::bold));
        g.drawText(tr("Po R: ") + places((int)list.size()) + tr(" do wycięcia"),
                   getLocalBounds().reduced(8).removeFromTop(28), juce::Justification::centredLeft);
        g.setColour(Ink::textDim()); g.setFont(juce::Font(zs::type::small));
        const juce::String hint = pending > 0
            ? tr("Posłuchaj miejsc oznaczonych „sprawdź”. Czego nie zatwierdzisz, zostanie w nagraniu.")
            : cut > 0 ? tr("Posłuchaj szwów. „Zastosuj” wytnie ") + places(cut) + tr(" — pełne nagranie zostanie w folderze surowe.")
                      : tr("Nic nie zostanie wycięte.");
        g.drawFittedText(hint, getLocalBounds().reduced(8).withTrimmedBottom(36).removeFromBottom(25),
                         juce::Justification::centredLeft, 1);
    }
    /** 1 miejsce, 2-4 miejsca, 5 miejsc (12-14 miejsc). */
    static juce::String places(int n)
    {
        const int d = n % 10, dd = n % 100;
        const char* w = n == 1 ? "miejsce" : (d >= 2 && d <= 4 && (dd < 12 || dd > 14)) ? "miejsca" : "miejsc";
        return juce::String(n) + " " + tr(w);
    }

    bool keyPressed(const juce::KeyPress& k) override
    {
        if (rows.empty() || !isEnabled()) return false;
        const int c = k.getKeyCode();
        if (c == juce::KeyPress::upKey || c == juce::KeyPress::downKey) {
            selected = juce::jlimit(0, (int)rows.size()-1, selected + (c == juce::KeyPress::upKey ? -1 : 1));
            viewport.setViewPosition(0, rows[(size_t)selected]->getY()); update(); return true;
        }
        auto& row = *rows[(size_t)selected];
        if (c == juce::KeyPress::spaceKey) { row.listen.onClick(); return true; }
        if (c == juce::KeyPress::returnKey) { row.accept(); return true; }
        if (c == juce::KeyPress::backspaceKey) { row.reject(); return true; }
        return false;
    }

private:
    struct Row : juce::Component
    {
        CutReviewPanel& owner;
        size_t index;
        double from, to, left, right;
        std::vector<double> snaps;
        zs::Envelope env;
        juce::TextButton listen, yes, no;
        juce::Rectangle<int> wave;
        bool draggingStart = false;
        bool dragging = false;
        Row(CutReviewPanel& o, size_t i, const std::vector<float>& audio, double sr)
            : owner(o), index(i)
        {
            const auto& p = owner.list[index];
            const double total = audio.size()/sr;
            from = juce::jlimit(0.0, total, p.from); to = juce::jlimit(from, total, p.to);
            left = juce::jmax(0.0, from-3); right = juce::jmin(total, to+3);
            if (!p.sure && right > left) {
                const size_t a = (size_t)(left*sr), b = juce::jmin(audio.size(), (size_t)(right*sr));
                std::vector<float> fragment(audio.begin()+a, audio.begin()+b);
                env.build(fragment, sr, 800);
                tw::CutFinder finder; finder.analyse(fragment.data(), fragment.size(), sr);
                for (const auto& pause : finder.pauses()) snaps.push_back(left + std::max(pause.first, pause.second-0.15));
            }
            setWantsKeyboardFocus(false);
            listen.setButtonText(p.sure ? tr("Posłuchaj szwu") : tr("Posłuchaj"));
            listen.onClick = [this] { select(); if(owner.onListen) owner.onListen(from,to); };
            yes.onClick = [this] {
                if(owner.list[index].sure && owner.list[index].state == tw::CutProposal::State::Accepted) reject();
                else accept();
            };
            no.setButtonText(tr("Zostaw")); no.onClick = [this] { reject(); };
            for (auto* b : {&listen, &yes, &no}) { b->setWantsKeyboardFocus(false); addAndMakeVisible(b); }
            no.setVisible(!p.sure);
        }
        void select() { owner.selected=(int)index; owner.grabKeyboardFocus(); owner.update(); }
        void accept() {
            select(); if (to <= from) return;
            owner.change(index, [this](auto& p) { if(p.sure) p.state=tw::CutProposal::State::Accepted; else p.moveTo(from,to); });
        }
        void reject() { select(); owner.change(index, [](auto& p) { p.state=tw::CutProposal::State::Rejected; }); }
        void resized() override {
            auto r=getLocalBounds().reduced(8);
            auto buttons=r.removeFromBottom(32);
            const int gap=6, w=(buttons.getWidth()-2*gap)/3;
            listen.setBounds(buttons.removeFromLeft(w)); buttons.removeFromLeft(gap);
            yes.setBounds(buttons.removeFromLeft(w)); buttons.removeFromLeft(gap); no.setBounds(buttons);
            r.removeFromTop(58); wave=r.withTrimmedBottom(8);
        }
        float x(double t) const { return wave.getX() + float((t-left)/juce::jmax(0.001,right-left))*wave.getWidth(); }
        void paint(juce::Graphics& g) override {
            const auto& p=owner.list[index];
            g.setColour(Ink::raised()); g.fillRoundedRectangle(getLocalBounds().toFloat(),4);
            if(owner.selected==(int)index) { g.setColour(Ink::green()); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),4,1); }
            auto r=getLocalBounds().reduced(8);
            g.setColour(Ink::textNext()); g.setFont(juce::Font(zs::type::small));
            auto clock=[](double s) { return juce::String::formatted("%d:%02d", (int)s/60,(int)s%60); };
            juce::String state = p.state==tw::CutProposal::State::Accepted ? tr("zostanie wycięte")
                               : p.state==tw::CutProposal::State::Rejected ? tr("zostaje w nagraniu") : tr("sprawdź");
            g.drawText(clock(from)+tr("–")+clock(to)+"  ("+juce::String(to-from,2)+" s) · "+state,r.removeFromTop(24),juce::Justification::centredLeft);
            if(p.sure) return;
            g.setColour(Ink::textDim());
            // Powod techniczny (p.why) zostaje w pliku .ciecia.tsv; tu zdanie dla czlowieka.
            g.drawFittedText(tr("Czerwone zostanie wycięte. Posłuchaj, czy nie ma tu dobrych słów — "
                                "uchwyty przesuwasz myszą."),r.removeFromTop(34),juce::Justification::topLeft,2);
            g.setColour(Ink::textNext().withAlpha(.65f));
            for(int px=0;px<wave.getWidth();px+=2) {
                if(env.peakDb.empty()) break;
                size_t j=juce::jmin(env.peakDb.size()-1,(size_t)(double(px)/juce::jmax(1,wave.getWidth())*env.peakDb.size()));
                float h=juce::jlimit(0.f,1.f,(env.peakDb[j]+66)/66)*wave.getHeight()*.45f;
                g.fillRect(float(wave.getX()+px),wave.getCentreY()-h,1.f,2*h);
            }
            g.setColour(Ink::red().withAlpha(.25f));
            g.fillRect(x(from),float(wave.getY()),juce::jmax(0.f,x(to)-x(from)),float(wave.getHeight()));
            g.setColour(Ink::red());
            for(double t : {from,to}) { g.fillRect(x(t)-2,float(wave.getY()),4.f,float(wave.getHeight())); g.fillEllipse(x(t)-6,float(wave.getY()),12.f,12.f); }
        }
        void mouseDown(const juce::MouseEvent& e) override {
            select(); dragging=wave.contains(e.getPosition()) && !owner.list[index].sure;
            draggingStart=std::abs(e.position.x-x(from))<=std::abs(e.position.x-x(to));
            if(dragging) mouseDrag(e);
        }
        void mouseDrag(const juce::MouseEvent& e) override {
            if(!dragging || right<=left) return;
            double t=left+juce::jlimit(0.0,1.0,double(e.position.x-wave.getX())/juce::jmax(1,wave.getWidth()))*(right-left);
            if(!snaps.empty()) {
                const double nearest=*std::min_element(snaps.begin(),snaps.end(),[t](double a,double b){return std::abs(a-t)<std::abs(b-t);});
                if(std::abs(nearest-t)*wave.getWidth()/(right-left)<=12) t=nearest;
            }
            if(draggingStart) from=juce::jmin(t,to); else to=juce::jmax(t,from);
            // Kazda nowa granica jest zapisana jako szkic do ponownej decyzji.
            owner.change(index,[this](auto& p) {
                p.from=from; p.to=to; p.state=tw::CutProposal::State::Pending;
            });
            from=owner.list[index].from; to=owner.list[index].to;
            repaint();
        }
        void mouseUp(const juce::MouseEvent&) override { dragging=false; }
    };
    template<class F> void change(size_t i, F mutate) {
        auto previous=list; mutate(list[i]);
        if(!zs::writeCutList(file,list)) {
            list=std::move(previous);
            if(onError) onError(tr("Nie udało się zapisać decyzji. Sprawdź dostęp do folderu nagrania."));
        }
        update();
    }
    void update() {
        apply.setEnabled(!empty());
        apply.setTooltip(decided()?juce::String():tr("Niezatwierdzone miejsca zostaną w nagraniu"));
        for(auto& row : rows) {
            const auto& p=list[row->index];
            row->yes.setButtonText(p.sure ? (p.state==tw::CutProposal::State::Accepted ? tr("Przywróć") : tr("Wytnij")) : tr("Wytnij"));
            row->yes.setEnabled(row->to>row->from); row->repaint();
        }
        repaint();
    }
    juce::File file;
    std::vector<tw::CutProposal> list;
    int selected=0;
    juce::Component body;
    juce::Viewport viewport;
    juce::TextButton apply;
    std::vector<std::unique_ptr<Row>> rows;
};
