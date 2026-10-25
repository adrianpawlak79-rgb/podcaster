/*
    Main.cpp — punkt wejscia programu "Zielone swiatlo".
*/

#include "BinaryData.h"
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
        // --paleta studio | wtyczka | granat | miedz | klasyczna (domyslnie studio) — takze w --preview-ui
        {
            const auto a = juce::StringArray::fromTokens (commandLine, true);
            const int i = a.indexOf ("--paleta");
            const auto v = i >= 0 && i + 1 < a.size() ? a[i + 1] : juce::String();
            zs::ui::setPalette (v == "miedz" ? zs::ui::PaletteId::Copper
                              : v == "klasyczna" ? zs::ui::PaletteId::Classic
                              : v == "granat" ? zs::ui::PaletteId::Console
                              : v == "wtyczka" ? zs::ui::PaletteId::Plugin : zs::ui::PaletteId::Studio);
            look.applyPalette();
        }
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
"Active output channels:" = "Kanały słuchawek:"
"Active input channels:" = "Kanały mikrofonu:"
"Show advanced settings..." = "Więcej ustawień…"
"Hide advanced settings" = "Mniej ustawień"
"Reset Device" = "Uruchom ponownie"
"Device:" = "Urządzenie:"
"Plays a test tone" = "Odtwarza krótki dźwięk w słuchawkach"
"Opens the device's own control panel" = "Otwiera ustawienia karty dźwiękowej"
"Error when trying to open audio device!" = "Nie udało się otworzyć karty dźwiękowej"
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
            if (mode.endsWith ("-fhd"))   { preview.setSize (1920, 1080); mode = mode.dropLastCharacters (4); }
            if(mode=="settings") preview.previewSettings();
            if(mode=="prompter")preview.previewPrompter();
            if(mode=="small") preview.setSize(960,640);
            if(mode=="wizard") preview.previewWizard();
            if(mode=="recording") preview.previewRecording(false);
            if(mode=="prompter-rec") preview.previewRecording(true);
            if(mode=="prompter-znaczki") { preview.previewRecording(true); preview.previewMarks(); }
            if(mode=="prompter-wskazanie") { preview.previewRecording(true); preview.previewAim(); }
            if(mode=="prompter-pasek") { preview.previewRecording(true); preview.previewLive(); }
            if(mode=="review-ciecia") {
                if(!args[2].endsWith("-small")) preview.setSize(1280,1100);
                preview.previewCuts(fixture);
            }
            if(mode=="witaj") preview.previewStart(fixture.getParentDirectory());
            if(mode=="review") preview.previewReview(fixture);
            if(mode=="tor") preview.previewChain(fixture);
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

        // Ekran startowy na 2,5 s (klik zamyka wczesniej) — tylko przy zwyklym
        // uruchomieniu, nie przy --ocena (zrzuty ekranu przy budowaniu).
        if (review == juce::File())
        {
            auto img = juce::ImageFileFormat::loadFrom (BinaryData::splash_jpg, (size_t) BinaryData::splash_jpgSize);
            if (img.isValid())
            {
                const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                                ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                                : juce::Rectangle<int> (1280, 800);
                const int w = juce::jmin (960, area.getWidth() * 2 / 3);
                img = img.rescaled (w, w * img.getHeight() / img.getWidth(), juce::Graphics::highResamplingQuality);
                auto* splash = new juce::SplashScreen ("Zielone swiatlo", img, true);
                splash->deleteAfterDelay (juce::RelativeTime::seconds (2.5), true);
            }
        }
    }

    void shutdown() override { mainWindow = nullptr; juce::LookAndFeel::setDefaultLookAndFeel(nullptr); }
    // Kazda prosba o zamkniecie (X, Alt+F4, wylogowanie) idzie przez
    // MainComponent::requestClose — nagranie i niezapisany montaz nie gina po cichu.
    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            if (auto* main = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            {
                main->requestClose ([] { juce::MessageManager::callAsync ([] { if (auto* app = JUCEApplicationBase::getInstance()) app->quit(); }); });
                return;
            }
        quit();
    }

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
            // Gorny limit wiekszy niz monitory: przy 2400 x 1600 okno na ekranie
            // 2560 x 1440 albo 4K nie dawalo sie zmaksymalizowac do konca.
            setResizeLimits (960, 640, 8192, 8192);

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
            {
                // Pierwsze uruchomienie: na caly ekran (zmaksymalizowane), jak
                // programy do dzwieku — prompter i przebieg potrzebuja miejsca.
                // Potem program pamieta to, co ustawil czlowiek.
                centreWithSize (getWidth(), getHeight());
                setFullScreen (true);
            }

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
