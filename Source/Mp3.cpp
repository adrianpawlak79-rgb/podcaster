/* Mp3.cpp — zapis MP3 przez Media Foundation. Opis w Mp3.h. */

#include "Mp3.h"
#include "Wav.h"

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <type_traits>

#if defined (_WIN32)
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>
#endif

namespace zs
{
namespace mp3
{

//==============================================================================
#if defined (_WIN32)

namespace detail
{
    template <typename T> struct Com
    {
        T* p = nullptr;
        ~Com() { if (p) p->Release(); }
        T** put() { if (p) { p->Release(); p = nullptr; } return &p; }
        T* operator->() const { return p; }
        explicit operator bool() const { return p != nullptr; }
    };

    /** Media Foundation ladowane dopiero przy pierwszym MP3, nie przy starcie.
        Import zwykly (mf.dll, mfplat.dll w tabeli importow .exe) sprawial, ze
        na Windows N bez Media Feature Pack program w ogole by sie nie
        uruchomil — a MP3 to dodatek do zapisanego juz WAV-a. */
    struct Api
    {
        bool ok = false;
        HRESULT (WINAPI* startup) (ULONG, DWORD) = nullptr;
        HRESULT (WINAPI* shutdown)() = nullptr;
        HRESULT (WINAPI* createAttributes) (IMFAttributes**, UINT32) = nullptr;
        HRESULT (WINAPI* createMediaType) (IMFMediaType**) = nullptr;
        HRESULT (WINAPI* createMemoryBuffer) (DWORD, IMFMediaBuffer**) = nullptr;
        HRESULT (WINAPI* createSample) (IMFSample**) = nullptr;
        HRESULT (WINAPI* createSinkWriter) (LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**) = nullptr;
        HRESULT (WINAPI* outputTypes) (REFGUID, DWORD, IMFAttributes*, IMFCollection**) = nullptr;

        Api()
        {
            HMODULE plat = LoadLibraryW (L"mfplat.dll"), rw = LoadLibraryW (L"mfreadwrite.dll"), mf = LoadLibraryW (L"mf.dll");
            if (! plat || ! rw || ! mf) return;
            const auto get = [] (HMODULE m, const char* name, auto& fn) { fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (reinterpret_cast<void*> (GetProcAddress (m, name))); return fn != nullptr; };
            ok = get (plat, "MFStartup", startup) && get (plat, "MFShutdown", shutdown)
              && get (plat, "MFCreateAttributes", createAttributes) && get (plat, "MFCreateMediaType", createMediaType)
              && get (plat, "MFCreateMemoryBuffer", createMemoryBuffer) && get (plat, "MFCreateSample", createSample)
              && get (rw, "MFCreateSinkWriterFromURL", createSinkWriter)
              && get (mf, "MFTranscodeGetAudioOutputAvailableTypes", outputTypes);
        }
    };

    inline const Api& api() { static const Api a; return a; }

    /** COM i Media Foundation na czas jednego zapisu, na biezacym watku. */
    struct Session
    {
        bool com = false, mf = false;
        Session()
        {
            const HRESULT c = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            com = SUCCEEDED (c);                     // RPC_E_CHANGED_MODE: COM juz jest, tez dobrze
            mf = api().ok && SUCCEEDED (api().startup (MF_VERSION, MFSTARTUP_LITE));
        }
        ~Session()
        {
            if (mf) api().shutdown();
            if (com) CoUninitialize();
        }
    };

    /** Typ wyjsciowy kodera: te kanaly i czestotliwosc, bitrate najblizszy zadanemu. */
    inline HRESULT pickOutputType (int channels, int rate, int kbps, IMFMediaType** out)
    {
        Com<IMFCollection> types;
        HRESULT hr = api().outputTypes (MFAudioFormat_MP3, MFT_ENUM_FLAG_ALL, nullptr, types.put());
        if (FAILED (hr)) return hr;

        DWORD count = 0;
        types->GetElementCount (&count);
        const UINT32 wantBytes = (UINT32) kbps * 1000 / 8;
        IMFMediaType* best = nullptr;
        UINT32 bestDiff = 0xffffffffu;

        for (DWORD i = 0; i < count; ++i)
        {
            Com<IUnknown> item;
            if (FAILED (types->GetElement (i, item.put()))) continue;
            Com<IMFMediaType> t;
            if (FAILED (item->QueryInterface (IID_PPV_ARGS (t.put())))) continue;

            UINT32 ch = 0, sr = 0, bytes = 0;
            t->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &ch);
            t->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr);
            t->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytes);
            if ((int) ch != channels || (int) sr != rate) continue;

            const UINT32 diff = bytes > wantBytes ? bytes - wantBytes : wantBytes - bytes;
            if (diff < bestDiff)
            {
                if (best) best->Release();
                best = t.p; best->AddRef();
                bestDiff = diff;
            }
        }
        if (best == nullptr) return MF_E_INVALIDMEDIATYPE;
        *out = best;
        return S_OK;
    }

    inline std::string describe (const char* what, HRESULT hr)
    {
        char code[16];
        std::snprintf (code, sizeof (code), "0x%08lX", (unsigned long) hr);
        return std::string (what) + " (" + code + ")";
    }
}

bool available()
{
    detail::Session s;
    if (! s.mf) return false;
    detail::Com<IMFMediaType> t;
    return SUCCEEDED (detail::pickOutputType (1, 48000, 128, t.put()))
        || SUCCEEDED (detail::pickOutputType (1, 44100, 128, t.put()));
}

std::string write (const std::string& path, const std::vector<const float*>& channels,
                   size_t frames, int sampleRate, int kbps, double* progress)
{
    using detail::Com;
    const int ch = (int) channels.size();
    if (ch < 1 || ch > 2 || frames == 0) return "nie ma czego zapisac";
    if (sampleRate != 44100 && sampleRate != 48000 && sampleRate != 32000)
        return "MP3 przyjmuje 32, 44,1 albo 48 kHz";

    detail::Session session;
    if (! session.mf)
        return "brak Media Foundation w systemie (Windows N: doinstaluj Media Feature Pack)";

    Com<IMFMediaType> outType;
    HRESULT hr = detail::pickOutputType (ch, sampleRate, kbps, outType.put());
    if (FAILED (hr)) return detail::describe ("brak kodera MP3 w systemie", hr);

    const std::string temp = path + ".zapis.tmp";
    wavio::detail::removeFile (temp);

    Com<IMFAttributes> attrs;
    detail::api().createAttributes (attrs.put(), 1);
    attrs->SetGUID (MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MP3);

    Com<IMFSinkWriter> writer;
    hr = detail::api().createSinkWriter (wavio::detail::widen (temp).c_str(), nullptr, attrs.p, writer.put());
    if (FAILED (hr)) return detail::describe ("nie moge utworzyc pliku MP3", hr);

    DWORD stream = 0;
    hr = writer->AddStream (outType.p, &stream);
    if (FAILED (hr)) return detail::describe ("koder MP3 nie przyjal formatu", hr);

    Com<IMFMediaType> inType;
    detail::api().createMediaType (inType.put());
    inType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, (UINT32) ch);
    inType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) sampleRate);
    inType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    inType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, (UINT32) ch * 2);
    inType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) (sampleRate * ch * 2));
    inType->SetUINT32 (MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    hr = writer->SetInputMediaType (stream, inType.p, nullptr);
    if (FAILED (hr)) return detail::describe ("koder MP3 nie przyjal dzwieku", hr);

    hr = writer->BeginWriting();
    if (FAILED (hr)) return detail::describe ("nie moge zaczac zapisu MP3", hr);

    // dither TPDF: suma dwoch szumow rownomiernych, amplituda 1 LSB
    uint32_t seed = 0x9e3779b9u;
    auto noise = [&seed]
    {
        seed = seed * 1664525u + 1013904223u; const float a = (float) (seed >> 8) / 16777216.0f;
        seed = seed * 1664525u + 1013904223u; const float b = (float) (seed >> 8) / 16777216.0f;
        return a - b;
    };

    const size_t block = (size_t) sampleRate;    // sekunda na probke Media Foundation
    bool ok = true;
    for (size_t at = 0; at < frames && ok; at += block)
    {
        const size_t n = std::min (block, frames - at);
        const DWORD bytes = (DWORD) (n * (size_t) ch * 2);

        Com<IMFMediaBuffer> buffer;
        if (FAILED (detail::api().createMemoryBuffer (bytes, buffer.put()))) { ok = false; break; }
        BYTE* dst = nullptr;
        if (FAILED (buffer->Lock (&dst, nullptr, nullptr))) { ok = false; break; }
        auto* out = reinterpret_cast<int16_t*> (dst);
        for (size_t i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
            {
                float v = channels[(size_t) c][at + i];
                if (! (v == v)) v = 0.0f;
                v = v * 32767.0f + noise();
                v = v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v;
                *out++ = (int16_t) std::lround (v);
            }
        buffer->Unlock();
        buffer->SetCurrentLength (bytes);

        Com<IMFSample> sample;
        if (FAILED (detail::api().createSample (sample.put()))) { ok = false; break; }
        sample->AddBuffer (buffer.p);
        sample->SetSampleTime ((LONGLONG) ((double) at * 1.0e7 / sampleRate));
        sample->SetSampleDuration ((LONGLONG) ((double) n * 1.0e7 / sampleRate));
        if (FAILED (writer->WriteSample (stream, sample.p))) { ok = false; break; }

        if (progress != nullptr) *progress = (double) (at + n) / (double) frames;
    }

    const HRESULT fin = writer->Finalize();
    writer.put();                                 // zamyka plik przed zmiana nazwy
    if (! ok || FAILED (fin))
    {
        wavio::detail::removeFile (temp);
        return ok ? detail::describe ("nie moge zamknac pliku MP3", fin)
                  : "zapis MP3 sie nie udal (pelny dysk albo brak dostepu): " + path;
    }
    if (! wavio::detail::renameFile (temp, path))
    {
        wavio::detail::removeFile (temp);
        return "nie moge zapisac MP3 pod wlasciwa nazwa: " + path;
    }
    return {};
}

//==============================================================================
#else

bool available() { return false; }

std::string write (const std::string&, const std::vector<const float*>&, size_t, int, int, double*)
{
    return "MP3 tylko w wersji dla Windows";
}

#endif

} // namespace mp3
} // namespace zs
