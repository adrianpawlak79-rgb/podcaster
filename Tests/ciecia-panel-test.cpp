// Prawdziwe przyciski i klawiatura panelu, zapis TSV oraz WAV pozostawiony
// bez zmian do wywolania callbacku zastosowania. Bez karty dzwiekowej.
#include "CutReviewPanel.h"
#include "Wav.h"
#include <iostream>

static juce::String PL(const char* s) { return juce::String::fromUTF8(s); }
static int bad=0;
static void check(bool ok,const char* text) { std::cout<<(ok?"OK ":"ZLE ")<<text<<"\n"; if(!ok) ++bad; }
static juce::Button* button(juce::Component& c,const juce::String& text,int occurrence=0)
{
    for(int i=0;i<c.getNumChildComponents();++i) {
        auto* child=c.getChildComponent(i);
        if(auto* b=dynamic_cast<juce::Button*>(child))
            if(b->getButtonText()==text && occurrence--==0) return b;
        if(auto* found=button(*child,text,occurrence)) return found;
    }
    return nullptr;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zs-panel-test","",false);
    dir.createDirectory(); auto wav=dir.getChildFile("sesja.wav");
    std::vector<float> audio(80000,0.f);
    // Pauza 4.0-4.4 s, a za nia mowa: przyciaganie konca pauzy - 0.15 s.
    for(size_t i=44000;i<80000;++i) audio[i]=.2f*std::sin(double(i)*.1);
    wavio::Audio out; out.sampleRate=10000; out.channels.push_back(audio);
    check(wavio::write24(wav.getFullPathName().toStdString(),out).empty(),"plik testowy");
    std::vector<tw::CutProposal> list(3);
    list[0].from=1; list[0].to=2; list[0].sure=true; list[0].state=tw::CutProposal::State::Accepted;
    list[1]=list[0]; list[1].from=2; list[1].to=3;
    list[2].from=4; list[2].to=5; list[2].why="Niepewny początek";
    zs::writeCutList(wav,list);
    juce::MemoryBlock before; wav.loadFileAsData(before);
    CutReviewPanel panel; panel.setSize(900,600); panel.load(wav,audio,10000);
    check(!panel.decided(),"jest miejsce do sprawdzenia");
    auto* apply=button(panel,PL("Wytnij zaznaczone (2)"));
    check(apply && apply->isEnabled(),"Wytnij zaznaczone dziala od razu i mowi, ile wytnie");
    auto* restore=button(panel,PL("Odznacz")); restore->onClick();
    check(zs::readCutList(wav)[0].state==tw::CutProposal::State::Rejected,"Odznacz zapisuje Rejected");
    check(apply->getButtonText()==PL("Wytnij zaznaczone (1)"),"licznik na przycisku spada");
    auto* cut=button(panel,PL("Zaznacz do wycięcia")); cut->onClick();
    check(zs::readCutList(wav)[0].state==tw::CutProposal::State::Accepted,"Zaznacz do wyciecia zapisuje Accepted");
    check(juce::File(wav).existsAsFile() && apply->getButtonText()==PL("Wytnij zaznaczone (2)"),"zaznaczenie nie tnie, tylko liczy");
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    int heard=0; double heardFrom=0,heardTo=0;
    panel.onListen=[&](double a,double b){++heard;heardFrom=a;heardTo=b;};
    panel.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
    check(heard==1 && heardFrom==4 && heardTo==5,"strzalki i Spacja wybieraja trzeci fragment");
    panel.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    check(panel.decided() && apply->isEnabled(),"Enter akceptuje");
    check(zs::readCutList(wav)[2].state==tw::CutProposal::State::Accepted,"Tnij tu zapisane w TSV");
    // Mysza przesuwamy poczatek w poblize 4.25 s. Pauza 0-4.4 s
    // przyciaga go do 4.25 s; przesuniecie cofa Accepted do Pending.
    auto* row=button(panel,PL("Zaznaczone"))->getParentComponent();
    const float width=float(row->getWidth()-16);
    auto event=[&](double t) {
        juce::Point<float> pt(8+float((t-1)/7)*width,90);
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pt,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
            row,row,juce::Time::getCurrentTime(),pt,juce::Time::getCurrentTime(),1,true);
    };
    row->mouseDown(event(4)); row->mouseDrag(event(4.27)); row->mouseUp(event(4.27));
    auto dragged=zs::readCutList(wav)[2];
    check(std::abs(dragged.from-4.25)<.011 && dragged.state==tw::CutProposal::State::Pending,
          "przeciagniecie przyciaga do pauzy i zapisuje Pending");
    check(!panel.decided(),"przesuniecie wymaga ponownego zatwierdzenia");
    panel.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    panel.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));
    check(zs::readCutList(wav)[2].state==tw::CutProposal::State::Rejected,"Backspace zostawia obie wersje");
    panel.load(wav,audio,10000);
    check(panel.decided(),"decyzje zachowane po ponownym otwarciu");
    juce::MemoryBlock after; wav.loadFileAsData(after);
    check(before==after,"decyzje i odsluch nie zmieniaja WAV");
    int applied=0; zs::PunchResult result;
    panel.onApply=[&](const auto& proposals){++applied;result=zs::applyCutList(wav,proposals);};
    button(panel,PL("Wytnij zaznaczone (2)"))->onClick();
    check(applied==1 && result.applied && result.cuts==1,"dopiero Zastosuj wycina zaakceptowane (scalone) fragmenty");
    check(result.rawCopy.existsAsFile() && zs::readCutList(wav).empty(),"surowe i lista zarchiwizowane");
    panel.load(wav,audio,10000); check(panel.empty(),"po sukcesie panel znika");
    // Blad zapisu nie moze zostawic pozornie zatwierdzonej decyzji.
    auto pending=list; pending.resize(1); pending[0].sure=false; pending[0].state=tw::CutProposal::State::Pending;
    auto blocked=dir.getChildFile("blokada.wav"); zs::writeCutList(blocked,pending);
    panel.load(blocked,audio,10000); zs::cutListFile(blocked).deleteFile(); zs::cutListFile(blocked).createDirectory(); zs::cutListFile(blocked).getChildFile("przeszkoda").replaceWithText("blokada");
    int errors=0; panel.onError=[&](const auto&){++errors;};
    panel.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    check(errors==1 && !panel.decided(),"blad zapisu cofa decyzje i pokazuje komunikat");
    dir.deleteRecursively(); return bad ? 1 : 0;
}
