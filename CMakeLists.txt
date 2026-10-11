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
    BUNDLE_ID "pl.zieloneswiatlo.recorder")

target_sources(ZieloneSwiatlo PRIVATE Source/Main.cpp)

target_include_directories(ZieloneSwiatlo PRIVATE Source)

target_compile_definitions(ZieloneSwiatlo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_ASIO=0
    JUCE_USE_WINRT_MIDI=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(ZieloneSwiatlo
    PRIVATE juce::juce_audio_utils juce::juce_gui_extra
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
