/*
    Main.cpp — punkt wejscia programu "Zielone swiatlo".
*/

#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class ZieloneSwiatloApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Zielone swiatlo"; }
    const juce::String getApplicationVersion() override    { return "0.4.0-test"; }
    bool moreThanOneInstanceAllowed() override { return getCommandLineParameters().startsWith("--preview-ui"); }

    void initialise (const juce::String& commandLine) override
    {
        // --ocena plik.wav — otwiera ekran po nagraniu na gotowym pliku.
        // Sluzy dwóm rzeczom: obejrzeniu wczorajszego nagrania i sprawdzeniu
        // tego ekranu przy budowaniu, bez prawdziwego mikrofonu.
        juce::LookAndFeel::setDefaultLookAndFeel(&look);
        juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(PL(R"(
language: Polish
countries: pl
"Audio device type:" = "Typ urządzenia:"
"Output:" = "Słuchawki:"
"Input:" = "Mikrofon:"
"Sample rate:" = "Częstotliwość:"
"Audio buffer size:" = "Bufor dźwięku:"
"Test" = "Test"
"none" = "brak urządzenia"
"Control Panel" = "Panel urządzenia"
"Show this device's control panel" = "Otwórz ustawienia urządzenia"
)"),false));
        juce::File review;
        bool waitForIt = false;

        const auto args = juce::StringArray::fromTokens (commandLine, true);

        if(args.size()>=2 && args[0]=="--preview-ui") {
            const auto fixture=args.size()>3?juce::File::getCurrentWorkingDirectory().getChildFile(args[3].unquoted()):juce::File();
            MainComponent preview(fixture,true,true);
            auto mode = args.size() > 2 ? args[2] : juce::String();
            if (mode.endsWith ("-small")) { preview.setSize (960, 640); mode = mode.dropLastCharacters (6); }
            if(mode=="settings") { preview.setSize(960,640);preview.previewSettings(); }
            if(mode=="prompter")preview.previewPrompter();
            if(mode=="small") preview.setSize(960,640);
            if(mode=="wizard") preview.previewWizard();
            if(mode=="recording") preview.previewRecording(false);
            if(mode=="prompter-rec") preview.previewRecording(true);
            if(mode=="prompter-znaczki") { preview.previewRecording(true); preview.previewMarks(); }
            if(mode=="review-ciecia") {
                if(!args[2].endsWith("-small")) preview.setSize(1280,1100);
                preview.previewCuts(fixture);
            }
            if(mode=="review") preview.previewReview(fixture);
            if(mode=="edit") preview.previewReview(fixture,true);
            if(mode=="done") preview.previewReview(fixture,false,true);
            if(mode=="review-big") { preview.setSize(1600,1200); preview.previewReview(fixture); }
            auto file=juce::File::getCurrentWorkingDirectory().getChildFile(args[1].unquoted());
            file.getParentDirectory().createDirectory();
            if(auto stream=file.createOutputStream()) {
                stream->setPosition(0);stream->truncate();
                juce::PNGImageFormat png;
                png.writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()),*stream);
            }
            quit();return;
        }
        for (int i = 0; i < args.size() - 1; ++i)
        {
            // --ocena-teraz liczy miks przed pokazaniem okna. Sluzy robieniu
            // zrzutow ekranu przy budowaniu — patrz ReviewComponent::load().
            if (args[i] == "--ocena" || args[i] == "--ocena-teraz")
            {
                review = juce::File::getCurrentWorkingDirectory()
                             .getChildFile (args[i + 1].unquoted());

                waitForIt = args[i] == "--ocena-teraz";
            }
        }

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), review, waitForIt);
    }

    void shutdown() override { mainWindow = nullptr; juce::LookAndFeel::setDefaultLookAndFeel(nullptr); }
    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, const juce::File& openForReview = {},
                    bool reviewNow = false)
            : DocumentWindow (name,
                              zs::Ink::bg(),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            auto* main = new MainComponent (openForReview, reviewNow);
            settings = &main->userSettings();

            setContentOwned (main, true);
            setResizable (true, false);
            setResizeLimits (960, 640, 2400, 1600);

            // Polozenie okna.
            //
            // Prompter czyta sie z monitora, ktory czesto nie jest tym samym, na
            // ktorym stoi reszta pracy — a przeciaganie okna na drugi ekran przed
            // kazdym nagraniem to ta sama drobna upokarzajaca czynnosc co
            // szukanie pliku z tekstem. Zapisane polozenie moze wskazywac monitor,
            // ktorego juz nie ma, wiec JUCE sprawdza to samo i w razie czego
            // ustawia okno na widocznym ekranie.
            if (settings != nullptr && settings->windowState().isNotEmpty())
                restoreWindowStateFromString (settings->windowState());
            else
                centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        ~MainWindow() override
        {
            if (settings != nullptr)
            {
                settings->setWindowState (getWindowStateAsString());
                settings->flush();
            }
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        Settings* settings = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    zs::Look look;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ZieloneSwiatloApplication)
