/*
    Version.h — numer wersji programu w jednym miejscu.

    Wartosc pochodzi z CMakeLists.txt (project(... VERSION ...)), ktory
    przekazuje ja jako ZS_VERSION. Testy kompilowane jednym g++ nie maja
    tej definicji, wiec jest wartosc zastepcza.
*/

#pragma once

#ifndef ZS_VERSION
 #define ZS_VERSION "0.0.0-dev"
#endif

namespace zs
{
inline const char* versionString() { return ZS_VERSION; }
}
