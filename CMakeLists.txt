cmake_minimum_required(VERSION 3.22)
project(ZieloneSwiatlo VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(ZS_JUCE_PATH "" CACHE PATH "Path to a JUCE 7 checkout")

if(ZS_JUCE_PATH AND EXISTS "${ZS_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${ZS_JUCE_PATH}" JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE7/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE7" JUCE)
else()
    message(FATAL_ERROR "Podaj -DZS_JUCE_PATH=/sciezka/do/JUCE7")
endif()

juce_add_gui_app(ZieloneSwiatlo
    VERSION ${PROJECT_VERSION}
    PRODUCT_NAME "Zielone swiatlo"
    COMPANY_NAME "Adrian Pawlak"
    BUNDLE_ID "pl.zieloneswiatlo.recorder"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/ikona.png")   # z Resources/ikona.svg

target_sources(ZieloneSwiatlo PRIVATE Source/Main.cpp Source/Mp3.cpp)

# Zapis MP3 koderem Windows (Media Foundation, Source/Mp3.cpp) — bez
# dodatkowych bibliotek w paczce; na Linuksie Mp3.cpp to zaslepka.
if (WIN32)
    target_link_libraries(ZieloneSwiatlo PRIVATE mfuuid ole32)   # same GUID-y; DLL laduje Mp3.cpp
endif()

target_include_directories(ZieloneSwiatlo PRIVATE Source)
target_compile_definitions(ZieloneSwiatlo PRIVATE ZS_FONTS=1)

target_compile_definitions(ZieloneSwiatlo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_ASIO=0
    JUCE_USE_WINRT_MIDI=0
    JUCE_USE_MP3AUDIOFORMAT=1   # podklad muzyczny z MP3 (Backing.h)
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# Ekran startowy: zdjecie od ChatGPT (Adrian: zostaje oryginal)
# (Docs/projekt/szata-chatgpt/).
# Fonty kierunku E (specyfikacja: Docs/projekt/ciemny-sprzet/): Inter do napisow,
# JetBrains Mono do liczb. Licencja OFL obok plikow. Look.h laduje je tylko
# w programie (ZS_FONTS) — testy JUCE zostaja na kroju systemowym.
juce_add_binary_data(ZsObrazy SOURCES Resources/splash.jpg
    Resources/Fonts/Inter-Regular.ttf Resources/Fonts/Inter-SemiBold.ttf
    Resources/Fonts/JetBrainsMono-SemiBold.ttf)

target_link_libraries(ZieloneSwiatlo
    PRIVATE ZsObrazy juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC  juce::juce_recommended_config_flags)

#==============================================================================
# Test pamietania ustawien.
#
# Reszta testow to zwykle pliki .cpp kompilowane jednym g++ (Tests/wszystko.sh),
# bo caly nasz DSP jest nagloweczny i nie potrzebuje JUCE. Ustawienia sa jedynym
# wyjatkiem: siedza na juce::ApplicationProperties, czyli na prawdziwym pliku w
# prawdziwym katalogu uzytkownika. Dlatego ten jeden test musi przejsc przez
# CMake'a — i dlatego uruchamia sie go pod Wine razem z programem, a nie
# natywnie: ma sprawdzic to, co sie stanie na Windowsie.
juce_add_console_app(UstawieniaTest PRODUCT_NAME "ustawienia-test")

target_sources(UstawieniaTest PRIVATE Tests/ustawienia-test.cpp)
target_include_directories(UstawieniaTest PRIVATE Source)

target_compile_definitions(UstawieniaTest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(UstawieniaTest
    PRIVATE juce::juce_data_structures juce::juce_events
    PUBLIC  juce::juce_recommended_config_flags)

# Punch and roll na prawdziwych plikach: czysty plik, surowe w "surowe",
# czasy wersow przeliczone. Zasady ciecia testuje punch-test bez JUCE.
juce_add_console_app(PunchPlikTest PRODUCT_NAME "punch-plik-test")
target_sources(PunchPlikTest PRIVATE Tests/punch-plik-test.cpp)
target_include_directories(PunchPlikTest PRIVATE Source)
target_compile_definitions(PunchPlikTest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(PunchPlikTest
    PRIVATE juce::juce_audio_formats juce::juce_events
    PUBLIC  juce::juce_recommended_config_flags)

juce_add_console_app(RuntimeTest PRODUCT_NAME "runtime-test")
target_sources(RuntimeTest PRIVATE Tests/runtime-test.cpp)
target_include_directories(RuntimeTest PRIVATE Source)
target_compile_definitions(RuntimeTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_WINRT_MIDI=0)
target_link_libraries(RuntimeTest PRIVATE juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags)

# Vosk cala droga (pierscien -> 16 kHz -> VoskEngine -> WordTracker). Potrzebuje
# libvosk i modelu w czasie uruchomienia (ZS_VOSK_LIB, ZS_VOSK_MODEL), nie do
# budowania — VoskEngine wczytuje je dopiero w trakcie.
juce_add_console_app(VoskSilnikTest PRODUCT_NAME "vosk-silnik-test")
target_sources(VoskSilnikTest PRIVATE Tests/vosk-silnik-test.cpp)
target_include_directories(VoskSilnikTest PRIVATE Source)
target_compile_definitions(VoskSilnikTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(VoskSilnikTest PRIVATE juce::juce_core
    PUBLIC juce::juce_recommended_config_flags)

if(MSVC)
    target_compile_options(ZieloneSwiatlo PRIVATE /utf-8)
    target_compile_options(RuntimeTest PRIVATE /utf-8)
endif()

# Symulacja calej sesji bez czlowieka (Tools/zs-scenariusz.py robi nagranie
# z zapisana prawda, to narzedzie puszcza je przez Recorder, prompter, lampke,
# R, wyciecie pomylek, ocene i miks — i pisze raport).
juce_add_console_app(ZsSymulacja PRODUCT_NAME "zs-symulacja")
target_sources(ZsSymulacja PRIVATE Tools/zs-symulacja.cpp)
target_include_directories(ZsSymulacja PRIVATE Source)
target_compile_definitions(ZsSymulacja PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_WINRT_MIDI=0)
target_link_libraries(ZsSymulacja PRIVATE juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags)

# Dluga sesja (60 min): pamiec, czas watku audio i klatki, wyciecie i miks na koncu.
juce_add_console_app(ZsDlugo PRODUCT_NAME "zs-dlugo")
target_sources(ZsDlugo PRIVATE Tools/zs-dlugo.cpp)
target_include_directories(ZsDlugo PRIVATE Source)
target_compile_definitions(ZsDlugo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_WINRT_MIDI=0)
target_link_libraries(ZsDlugo PRIVATE juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags)

# Panel decyzji po R: przyciski, klawiatura i zapis listy bez urzadzen audio.
juce_add_console_app(CieciaPanelTest PRODUCT_NAME "ciecia-panel-test")
target_sources(CieciaPanelTest PRIVATE Tests/ciecia-panel-test.cpp)
target_include_directories(CieciaPanelTest PRIVATE Source)
target_compile_definitions(CieciaPanelTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(CieciaPanelTest PRIVATE juce::juce_audio_formats juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags)
