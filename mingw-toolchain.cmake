# Cross-compiling RackRunner for 64-bit Windows from Linux with MinGW-w64.
#
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=Release -DRACK_JUCE_PATH=/path/to/JUCE-7
#
# NOTE: JUCE 8 dropped MinGW support, so this path needs JUCE 7.0.x.
# On Windows itself, build with Visual Studio instead - see README.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# the -posix variants provide std::thread / std::mutex / std::condition_variable
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Statically link the GCC runtime so the plug-in needs no MinGW DLLs beside it.
set(_fet76_static "-static-libgcc -static-libstdc++ -static")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_fet76_static}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_fet76_static}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_fet76_static}")

# CMake's default MinGW library list gets REPLACED by this variable, not
# appended to - so the whole Win32 set has to be repeated here, plus the extra
# libraries JUCE needs.
set(CMAKE_CXX_STANDARD_LIBRARIES
    "-Wl,-Bstatic -lwinpthread \
-lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid \
-lcomdlg32 -ladvapi32 -lwinmm -lversion -lwininet -lws2_32 -lwsock32 -limm32 \
-lshlwapi -lrpcrt4 -lodbc32 -lsetupapi -lmsimg32 -lopengl32 -ldbghelp -lcomctl32 \
-lmfplat -lmfuuid -lstrmiids -lwmcodecdspuuid -luxtheme -lcrypt32"
    CACHE STRING "" FORCE)

set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES}" CACHE STRING "" FORCE)
