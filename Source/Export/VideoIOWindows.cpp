// Media Foundation encoder/decoder (Windows 10/11). Built only on Windows.
#include "VideoIO.h"

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>
#include <propvarutil.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "propsys.lib")

namespace au
{
namespace
{
template <class T> void safeRelease (T*& p) { if (p) { p->Release(); p = nullptr; } }

struct MfInit
{
    bool ok = false;
    MfInit() { CoInitializeEx (nullptr, COINIT_MULTITHREADED); ok = SUCCEEDED (MFStartup (MF_VERSION, MFSTARTUP_FULL)); }
    ~MfInit() { if (ok) MFShutdown(); }
};

std::wstring widen (const std::string& s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar (CP_UTF8, 0, s.data(), (int) s.size(), nullptr, 0);
    std::wstring w ((size_t) n, L'\0');
    MultiByteToWideChar (CP_UTF8, 0, s.data(), (int) s.size(), w.data(), n);
    return w;
}

// Streaming linear-phase resampler (windowed sinc, 16 taps) for sample rates AAC does not accept.
class Resampler
{
public:
    void reset (double ratioIn) { ratio = ratioIn; pos = 0; hist.assign (16, 0.f); }
    // appends output for `n` new input samples
    void process (const float* in, int n, std::vector<float>& out)
    {
        buf.insert (buf.end(), hist.begin(), hist.end());
        buf.insert (buf.end(), in, in + n);
        const double limit = (double) (buf.size() - 16);
        while (pos + 8.0 < limit)
        {
            const double c = pos + 8.0;
            const int i0 = (int) std::floor (c);
            double acc = 0, wsum = 0;
            for (int k = -7; k <= 8; ++k)
            {
                const double x = (double) (i0 + k) - c;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (3.141592653589793 * x) / (3.141592653589793 * x);
                const double win = 0.5 + 0.5 * std::cos (3.141592653589793 * x / 8.5);
                acc += buf[(size_t) (i0 + k)] * sinc * win; wsum += sinc * win;
            }
            out.push_back ((float) (acc / wsum));
            pos += ratio;
        }
        const size_t consumed = buf.size() - 16;
        pos -= (double) consumed;
        hist.assign (buf.end() - 16, buf.end());
        buf.clear();
    }
private:
    double ratio = 1.0, pos = 0;
    std::vector<float> hist, buf;
};

std::string hr2s (HRESULT hr) { char b[32]; std::snprintf (b, sizeof b, "0x%08lX", (unsigned long) hr); return b; }
}

// ============================================================================ writer
class MfWriter : public VideoWriter
{
public:
    bool open (const VideoWriterSettings& s, std::string& err) override
    {
        st = s;
        if (! mf.ok) { err = "Media Foundation is not available"; return false; }
        outRate = (std::abs (s.sampleRate - 44100.0) < 1.0) ? 44100 : 48000;
        resampleRatio = s.sampleRate / outRate;
        for (auto& r : resamplers) r.reset (resampleRatio);

        IMFAttributes* attr = nullptr;
        MFCreateAttributes (&attr, 2);
        attr->SetUINT32 (MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attr->SetGUID (MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
        HRESULT hr = MFCreateSinkWriterFromURL (widen (s.path).c_str(), nullptr, attr, &writer);
        safeRelease (attr);
        if (FAILED (hr)) { err = "Cannot create MP4 file (" + hr2s (hr) + ")"; return false; }

        // ---- video
        IMFMediaType* vOut = nullptr; MFCreateMediaType (&vOut);
        vOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        vOut->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_H264);
        vOut->SetUINT32 (MF_MT_AVG_BITRATE, (UINT32) s.videoBitrate);
        vOut->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        vOut->SetUINT32 (MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
        if (s.width * s.height > 1920 * 1088) vOut->SetUINT32 (MF_MT_MPEG2_LEVEL, eAVEncH264VLevel5_2);
        MFSetAttributeSize (vOut, MF_MT_FRAME_SIZE, (UINT32) s.width, (UINT32) s.height);
        fpsNum = (UINT32) std::llround (s.fps * 1000.0); fpsDen = 1000;
        MFSetAttributeRatio (vOut, MF_MT_FRAME_RATE, fpsNum, fpsDen);
        MFSetAttributeRatio (vOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = writer->AddStream (vOut, &vStream);
        safeRelease (vOut);
        if (FAILED (hr)) { err = "H.264 encoder rejected " + std::to_string (s.width) + "x" + std::to_string (s.height) + " (" + hr2s (hr) + ")"; return false; }

        IMFMediaType* vIn = nullptr; MFCreateMediaType (&vIn);
        vIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        vIn->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        vIn->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize (vIn, MF_MT_FRAME_SIZE, (UINT32) s.width, (UINT32) s.height);
        MFSetAttributeRatio (vIn, MF_MT_FRAME_RATE, fpsNum, fpsDen);
        MFSetAttributeRatio (vIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        vIn->SetUINT32 (MF_MT_DEFAULT_STRIDE, (UINT32) (s.width * 4));
        hr = writer->SetInputMediaType (vStream, vIn, nullptr);
        safeRelease (vIn);
        if (FAILED (hr)) { err = "Video input format not supported (" + hr2s (hr) + ")"; return false; }

        // ---- audio
        if (s.channels > 0)
        {
            IMFMediaType* aOut = nullptr; MFCreateMediaType (&aOut);
            aOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            aOut->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
            aOut->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            aOut->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) outRate);
            aOut->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
            aOut->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
            hr = writer->AddStream (aOut, &aStream);
            safeRelease (aOut);
            if (FAILED (hr)) { err = "AAC encoder not available (" + hr2s (hr) + ")"; return false; }

            IMFMediaType* aIn = nullptr; MFCreateMediaType (&aIn);
            aIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            aIn->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
            aIn->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            aIn->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) outRate);
            aIn->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
            aIn->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
            aIn->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) outRate * 4);
            hr = writer->SetInputMediaType (aStream, aIn, nullptr);
            safeRelease (aIn);
            if (FAILED (hr)) { err = "Audio input format not supported (" + hr2s (hr) + ")"; return false; }
            hasAudio = true;
        }

        hr = writer->BeginWriting();
        if (FAILED (hr)) { err = "BeginWriting failed (" + hr2s (hr) + ")"; return false; }
        return true;
    }

    bool writeFrame (const uint8_t* px) override
    {
        const DWORD bytes = (DWORD) (st.width * st.height * 4);
        IMFMediaBuffer* buf = nullptr;
        if (FAILED (MFCreateMemoryBuffer (bytes, &buf))) return false;
        BYTE* dst = nullptr;
        buf->Lock (&dst, nullptr, nullptr);
        std::memcpy (dst, px, bytes);
        buf->Unlock();
        buf->SetCurrentLength (bytes);
        IMFSample* smp = nullptr; MFCreateSample (&smp);
        smp->AddBuffer (buf);
        const LONGLONG t0 = (LONGLONG) ((double) frame * 1.0e7 * fpsDen / fpsNum + 0.5);
        const LONGLONG t1 = (LONGLONG) ((double) (frame + 1) * 1.0e7 * fpsDen / fpsNum + 0.5);
        smp->SetSampleTime (t0); smp->SetSampleDuration (t1 - t0);
        const HRESULT hr = writer->WriteSample (vStream, smp);
        safeRelease (smp); safeRelease (buf);
        ++frame;
        return SUCCEEDED (hr);
    }

    bool writeAudio (const float* const* ch, int numCh, int n) override
    {
        if (! hasAudio || n <= 0) return true;
        // resample to 44.1/48k if needed
        const float* L = ch[0];
        const float* R = numCh > 1 ? ch[1] : ch[0];
        std::vector<float> l, r;
        int outN = n;
        if (std::abs (resampleRatio - 1.0) > 1e-9)
        {
            resamplers[0].process (L, n, l);
            resamplers[1].process (R, n, r);
            outN = (int) std::min (l.size(), r.size());
            L = l.data(); R = r.data();
        }
        if (outN <= 0) return true;
        const DWORD bytes = (DWORD) outN * 4;
        IMFMediaBuffer* buf = nullptr;
        if (FAILED (MFCreateMemoryBuffer (bytes, &buf))) return false;
        BYTE* dst = nullptr; buf->Lock (&dst, nullptr, nullptr);
        auto* d = reinterpret_cast<int16_t*> (dst);
        for (int i = 0; i < outN; ++i)
        {
            d[2 * i]     = (int16_t) std::clamp ((int) std::lrint (L[i] * 32767.0f), -32768, 32767);
            d[2 * i + 1] = (int16_t) std::clamp ((int) std::lrint (R[i] * 32767.0f), -32768, 32767);
        }
        buf->Unlock(); buf->SetCurrentLength (bytes);
        IMFSample* smp = nullptr; MFCreateSample (&smp); smp->AddBuffer (buf);
        const LONGLONG t0 = (LONGLONG) ((double) audioPos * 1.0e7 / outRate + 0.5);
        const LONGLONG t1 = (LONGLONG) ((double) (audioPos + outN) * 1.0e7 / outRate + 0.5);
        smp->SetSampleTime (t0); smp->SetSampleDuration (t1 - t0);
        const HRESULT hr = writer->WriteSample (aStream, smp);
        safeRelease (smp); safeRelease (buf);
        audioPos += outN;
        return SUCCEEDED (hr);
    }

    bool finish (std::string& err) override
    {
        if (! writer) return false;
        const HRESULT hr = writer->Finalize();
        safeRelease (writer);
        if (FAILED (hr)) { err = "Finalize failed (" + hr2s (hr) + ")"; return false; }
        return true;
    }
    const char* name() const override { return "Media Foundation"; }
    ~MfWriter() override { safeRelease (writer); }

private:
    MfInit mf;
    VideoWriterSettings st;
    IMFSinkWriter* writer = nullptr;
    DWORD vStream = 0, aStream = 0;
    UINT32 fpsNum = 30000, fpsDen = 1000;
    long long frame = 0, audioPos = 0;
    bool hasAudio = false;
    int outRate = 48000;
    double resampleRatio = 1.0;
    Resampler resamplers[2];
};

// ============================================================================ reader
class MfSource : public VideoSource
{
public:
    bool open (const std::string& p, std::string& err) override
    {
        if (! mf.ok) { err = "Media Foundation not available"; return false; }
        IMFAttributes* attr = nullptr; MFCreateAttributes (&attr, 1);
        attr->SetUINT32 (MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        HRESULT hr = MFCreateSourceReaderFromURL (widen (p).c_str(), attr, &reader);
        safeRelease (attr);
        if (FAILED (hr)) { err = "Cannot open video (" + hr2s (hr) + ")"; return false; }
        reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
        reader->SetStreamSelection ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        IMFMediaType* t = nullptr; MFCreateMediaType (&t);
        t->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        t->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = reader->SetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, t);
        safeRelease (t);
        if (FAILED (hr)) { err = "Cannot decode video to RGB (" + hr2s (hr) + ")"; return false; }
        IMFMediaType* cur = nullptr;
        reader->GetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur);
        UINT32 ww = 0, hh = 0; MFGetAttributeSize (cur, MF_MT_FRAME_SIZE, &ww, &hh);
        safeRelease (cur);
        w = (int) ww; h = (int) hh;
        if (w <= 0 || h <= 0) { err = "Bad video size"; return false; }
        PROPVARIANT var; PropVariantInit (&var);
        if (SUCCEEDED (reader->GetPresentationAttribute ((DWORD) MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)))
            dur = (double) var.uhVal.QuadPart / 1.0e7;
        PropVariantClear (&var);
        px.assign ((size_t) w * h * 4, 0);
        return true;
    }
    double duration() const override { return dur; }

    const uint8_t* frameAt (double t, int& ow, int& oh, bool& changed) override
    {
        ow = w; oh = h; changed = false;
        if (! reader) return px.data();
        if (dur > 0) { t = std::fmod (t, dur); if (t < 0) t += dur; }
        if (t < curT - 1e-3 || t > curT + 3.0 || eof)
        {
            PROPVARIANT var; InitPropVariantFromInt64 ((LONGLONG) (t * 1.0e7), &var);
            reader->SetCurrentPosition (GUID_NULL, var);
            PropVariantClear (&var);
            curT = -1; nextT = -1; eof = false;
        }
        int guard = 0;
        while ((nextT < 0 || nextT <= t) && ! eof && guard++ < 240)
        {
            DWORD flags = 0; LONGLONG ts = 0; IMFSample* s = nullptr;
            if (FAILED (reader->ReadSample ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &s))) { eof = true; break; }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { eof = true; safeRelease (s); break; }
            if (! s) continue;
            LONGLONG d = 0; s->GetSampleDuration (&d);
            copySample (s);
            safeRelease (s);
            curT = (double) ts / 1.0e7; nextT = curT + (d > 0 ? (double) d / 1.0e7 : 1.0 / 30.0);
            changed = true;
        }
        return px.data();
    }
    ~MfSource() override { safeRelease (reader); }

private:
    void copySample (IMFSample* s)
    {
        IMFMediaBuffer* buf = nullptr;
        if (FAILED (s->ConvertToContiguousBuffer (&buf))) return;
        IMF2DBuffer* b2 = nullptr;
        if (SUCCEEDED (buf->QueryInterface (IID_PPV_ARGS (&b2))))
        {
            BYTE* scan0 = nullptr; LONG pitch = 0;
            if (SUCCEEDED (b2->Lock2D (&scan0, &pitch)))
            {
                for (int y = 0; y < h; ++y)
                {
                    const BYTE* src = scan0 + (LONG) y * pitch;
                    uint8_t* dst = &px[(size_t) y * w * 4];
                    std::memcpy (dst, src, (size_t) w * 4);
                    for (int x = 0; x < w; ++x) dst[x * 4 + 3] = 255;
                }
                b2->Unlock2D();
            }
            safeRelease (b2);
        }
        else
        {
            BYTE* data = nullptr; DWORD len = 0;
            if (SUCCEEDED (buf->Lock (&data, nullptr, &len)))
            {
                // MF RGB32 without 2D buffer is bottom-up
                for (int y = 0; y < h && (DWORD) ((y + 1) * w * 4) <= len; ++y)
                {
                    uint8_t* dst = &px[(size_t) (h - 1 - y) * w * 4];
                    std::memcpy (dst, data + (size_t) y * w * 4, (size_t) w * 4);
                    for (int x = 0; x < w; ++x) dst[x * 4 + 3] = 255;
                }
                buf->Unlock();
            }
        }
        safeRelease (buf);
    }

    MfInit mf;
    IMFSourceReader* reader = nullptr;
    int w = 0, h = 0; double dur = 0, curT = -1, nextT = -1; bool eof = false;
    std::vector<uint8_t> px;
};

std::unique_ptr<VideoWriter> createMediaFoundationWriter() { return std::make_unique<MfWriter>(); }
std::unique_ptr<VideoSource> createMediaFoundationSource() { return std::make_unique<MfSource>(); }
}
#endif
