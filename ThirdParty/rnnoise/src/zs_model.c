/* zs_model.c — model RNNoise wbudowany w program (Zielone swiatlo).

   rnnoise_weights.bin to wagi z src/rnnoise_data.c (model 0a8755f8..., xiph/rnnoise
   70f1d25) zapisane programem dump_weights_blob z tego samego repo. Plik binarny
   ma 3,5 MB; ten sam model jako zrodlo C ma 75 MB.

   Wkompilowany dyrektywa .incbin (GCC/Clang, takze MinGW) — bez pobierania i bez
   generowania plikow w czasie budowania. ZS_RNNOISE_BLOB = sciezka do .bin
   (podaje ja CMakeLists.txt albo Tests/ci.sh). */

#include <stddef.h>

#ifndef ZS_RNNOISE_BLOB
#error "Podaj -DZS_RNNOISE_BLOB=\"/sciezka/ThirdParty/rnnoise/rnnoise_weights.bin\""
#endif

#if defined(_WIN32)
#define ZS_RODATA ".section .rdata,\"dr\"\n"
#elif defined(__APPLE__)
#define ZS_RODATA ".const_data\n"
#else
#define ZS_RODATA ".section .rodata\n"
#endif

#if defined(__APPLE__) || (defined(_WIN32) && !defined(_WIN64))
#define ZS_SYM(x) "_" #x
#else
#define ZS_SYM(x) #x
#endif

__asm__ (ZS_RODATA
         ".balign 64\n"
         ".globl " ZS_SYM(zs_rnnoise_blob) "\n"
         ZS_SYM(zs_rnnoise_blob) ":\n"
         ".incbin \"" ZS_RNNOISE_BLOB "\"\n"
         ".globl " ZS_SYM(zs_rnnoise_blob_end) "\n"
         ZS_SYM(zs_rnnoise_blob_end) ":\n"
         ".byte 0\n"
#if !defined(_WIN32) && !defined(__APPLE__)
         ".previous\n"
#else
         ".text\n"
#endif
         );

extern const unsigned char zs_rnnoise_blob[];
extern const unsigned char zs_rnnoise_blob_end[];

const void* zs_rnnoise_model_data (void) { return zs_rnnoise_blob; }
int zs_rnnoise_model_size (void) { return (int) (zs_rnnoise_blob_end - zs_rnnoise_blob); }
