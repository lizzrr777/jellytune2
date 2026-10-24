// JellyTuneDSP.h
// Motor de autotune: deteccion de pitch (YIN) + correccion con PSOLA.
// Codigo C++ puro (sin JUCE) para poder probarlo por separado.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace jelly
{
constexpr double kPi      = 3.14159265358979323846;
constexpr double kMinF0   = 80.0;    // Hz, voz mas grave que se detecta
constexpr double kMaxF0   = 1000.0;  // Hz, voz mas aguda que se detecta

// Interpolacion cubica (Hermite) sobre un buffer circular
inline double hermite (const float* buf, long long mask, double pos)
{
    const long long i = (long long) std::floor (pos);
    const double f = pos - (double) i;
    const double ym1 = buf[(i - 1) & mask];
    const double y0  = buf[i & mask];
    const double y1  = buf[(i + 1) & mask];
    const double y2  = buf[(i + 2) & mask];
    const double c1 = 0.5 * (y1 - ym1);
    const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

//==============================================================================
// Detector de pitch: YIN grueso en senal diezmada + refinado en senal completa
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        fs   = sampleRate;
        D    = std::max (1, (int) std::lround (fs / 12000.0));
        fsd  = fs / D;
        tauMinD = std::max (2, (int) std::floor (fsd / kMaxF0));
        tauMaxD = (int) std::ceil (fsd / kMinF0) + 1;
        Wd = 384;
        Nd = Wd + tauMaxD + 2;
        Wf = Wd * D;

        dec.assign ((size_t) Nd, 0.0);
        diff.assign ((size_t) tauMaxD + 2, 0.0);
        cm.assign ((size_t) tauMaxD + 2, 0.0);

        const int hiMax = tauMaxD * D + D + 2;
        seg.assign ((size_t) (Wf + hiMax + 8), 0.0f);
        dF.assign ((size_t) (2 * D + 8), 0.0);

        const int need = Wf + hiMax + 64;
        ringSize = 1;
        while (ringSize < need) ringSize <<= 1;
        mask = ringSize - 1;
        ring.assign ((size_t) ringSize, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writeIdx = 0;
    }

    void push (float x)
    {
        ring[(size_t) (writeIdx & mask)] = x;
        ++writeIdx;
    }

    // Devuelve la frecuencia fundamental en Hz, o 0 si no hay voz clara.
    double detect()
    {
        const int hiMax = tauMaxD * D + D + 2;
        if (writeIdx < (long long) (Wf + hiMax + 8)) return 0.0;

        const long long end = writeIdx;

        // 1) senal diezmada (promedio de D muestras)
        for (int k = 0; k < Nd; ++k)
        {
            const long long start = end - (long long) (Nd - k) * D;
            double s = 0.0;
            for (int q = 0; q < D; ++q) s += ring[(size_t) ((start + q) & mask)];
            dec[(size_t) k] = s / D;
        }

        // nivel (RMS) de la parte reciente
        double e2 = 0.0;
        for (int k = Nd - Wd; k < Nd; ++k) e2 += dec[(size_t) k] * dec[(size_t) k];
        const double rms = std::sqrt (e2 / Wd);
        if (rms < 0.002) return 0.0; // ~ -54 dBFS

        // 2) YIN: funcion de diferencia y CMND
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < Wd; ++j)
            {
                const double d = dec[(size_t) j] - dec[(size_t) (j + tau)];
                sum += d * d;
            }
            diff[(size_t) tau] = sum;
        }
        cm[0] = 1.0;
        double run = 0.0;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            run += diff[(size_t) tau];
            cm[(size_t) tau] = run > 1e-12 ? diff[(size_t) tau] * tau / run : 1.0;
        }

        int tauEst = -1;
        for (int tau = tauMinD; tau < tauMaxD; ++tau)
        {
            if (cm[(size_t) tau] < 0.15)
            {
                while (tau + 1 < tauMaxD && cm[(size_t) (tau + 1)] < cm[(size_t) tau]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0)
        {
            double best = 1e9;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
                if (cm[(size_t) tau] < best) { best = cm[(size_t) tau]; tauEst = tau; }
        }
        if (tauEst < 0 || cm[(size_t) tauEst] > 0.30) return 0.0;

        // 3) refinado en la senal a frecuencia de muestreo completa
        const int center = tauEst * D;
        const int lo = std::max (2, center - D - 1);
        const int hi = center + D + 1;
        const int segLen = Wf + hi;
        const long long a = end - segLen;
        for (int k = 0; k < segLen; ++k)
            seg[(size_t) k] = ring[(size_t) ((a + k) & mask)];

        int imin = lo;
        double dmin = 1e300;
        for (int tau = lo; tau <= hi; ++tau)
        {
            double sum = 0.0;
            const float* x0 = seg.data();
            const float* x1 = seg.data() + tau;
            for (int j = 0; j < Wf; ++j)
            {
                const double d = (double) x0[j] - (double) x1[j];
                sum += d * d;
            }
            dF[(size_t) (tau - lo)] = sum;
            if (sum < dmin) { dmin = sum; imin = tau; }
        }

        double tauF = (double) imin;
        if (imin > lo && imin < hi)
        {
            const double y0 = dF[(size_t) (imin - 1 - lo)];
            const double y1 = dF[(size_t) (imin - lo)];
            const double y2 = dF[(size_t) (imin + 1 - lo)];
            const double den = y0 - 2.0 * y1 + y2;
            if (den > 1e-15)
                tauF += std::clamp (0.5 * (y0 - y2) / den, -0.5, 0.5);
        }

        const double f0 = fs / tauF;
        if (f0 < kMinF0 * 0.97 || f0 > kMaxF0 * 1.03) return 0.0;
        return f0;
    }

private:
    double fs = 44100.0, fsd = 11025.0;
    int D = 4, tauMinD = 11, tauMaxD = 140, Wd = 384, Nd = 530, Wf = 1536;
    std::vector<double> dec, diff, cm, dF;
    std::vector<float> seg, ring;
    long long ringSize = 4096, mask = 4095, writeIdx = 0;
};

//==============================================================================
class JellyTuneDSP
{
public:
    static constexpr int kMaxChannels = 2;

    struct Settings
    {
        float retuneMs      = 25.0f;  // 0 = instantaneo (efecto robotico), 400 = natural
        int   rootPc        = 0;      // 0..11  (C=0, C#=1, ... B=11)
        int   scale         = 1;      // 0 = cromatica, 1 = mayor, 2 = menor
        bool  vibrato       = false;
        float vibRateHz     = 5.5f;
        float vibDepthCents = 35.0f;
    };

    void prepare (double sampleRate, int numChannels)
    {
        fs  = sampleRate;
        nCh = std::clamp (numChannels, 1, kMaxChannels);
        latency = (int) std::ceil (2.1 * fs / kMinF0) + 16;

        const int need = latency + (int) std::ceil (4.0 * fs / kMinF0) + 64;
        ringSize = 1;
        while (ringSize < need) ringSize <<= 1;
        mask = ringSize - 1;

        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            inRing[ch].assign ((size_t) ringSize, 0.0f);
            acc[ch].assign ((size_t) ringSize, 0.0f);
        }
        wsum.assign ((size_t) ringSize, 0.0f);

        det.prepare (fs);
        hopSamples = std::max (32, (int) std::lround (fs * 0.005));
        applySettings();
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            std::fill (inRing[ch].begin(), inRing[ch].end(), 0.0f);
            std::fill (acc[ch].begin(), acc[ch].end(), 0.0f);
        }
        std::fill (wsum.begin(), wsum.end(), 0.0f);
        det.reset();

        inIdx = 0; hopCount = 0;
        sNext = 0.0; cPrev = 0.0;
        voiced = false; voicedCnt = 0; unvoicedCnt = 0;
        histCount = 0; histPos = 0;
        hasTarget = false; target = 0;
        corr = 0.0; shiftSm = 0.0; vibPhase = 0.0;
        curT0 = fs * 0.008;
    }

    int getLatencySamples() const { return latency; }

    void setSettings (const Settings& s)
    {
        settings = s;
        applySettings();
    }

    // Procesa en el lugar. channels[ch][n]
    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int chans = std::min (numChannels, nCh);

        for (int n = 0; n < numSamples; ++n)
        {
            // 1) entrada
            float mono = 0.0f;
            for (int ch = 0; ch < chans; ++ch)
            {
                const float x = channels[ch][n];
                inRing[ch][(size_t) (inIdx & mask)] = x;
                mono += x;
            }
            det.push (mono / (float) std::max (1, chans));
            ++inIdx;

            if (++hopCount >= hopSamples)
            {
                hopCount = 0;
                updateControl (det.detect());
            }

            // 2) salida (retrasada 'latency' muestras)
            const long long j = inIdx - 1 - latency;
            if (j < 0)
            {
                for (int ch = 0; ch < chans; ++ch) channels[ch][n] = 0.0f;
                continue;
            }

            while (sNext - curT0 <= (double) j)
                placeGrain (j, chans);

            const size_t idx = (size_t) (j & mask);
            const float w = std::max (wsum[idx], 0.25f);
            for (int ch = 0; ch < chans; ++ch)
            {
                channels[ch][n] = acc[ch][idx] / w;
                acc[ch][idx] = 0.0f;
            }
            wsum[idx] = 0.0f;
        }
    }

private:
    //--------------------------------------------------------------------------
    void applySettings()
    {
        for (int i = 0; i < 12; ++i) allowed[i] = false;
        static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int root = ((settings.rootPc % 12) + 12) % 12;

        if (settings.scale == 1)
            for (int i = 0; i < 7; ++i) allowed[(root + major[i]) % 12] = true;
        else if (settings.scale == 2)
            for (int i = 0; i < 7; ++i) allowed[(root + minor[i]) % 12] = true;
        else
            for (int i = 0; i < 12; ++i) allowed[i] = true;
    }

    bool isAllowed (int note) const { return allowed[((note % 12) + 12) % 12]; }

    int nearestAllowed (double midi) const
    {
        const int base = (int) std::floor (midi);
        int best = base;
        double bestD = 1e9;
        for (int n = base - 3; n <= base + 4; ++n)
        {
            if (! isAllowed (n)) continue;
            const double d = std::abs (midi - (double) n);
            if (d < bestD) { bestD = d; best = n; }
        }
        return best;
    }

    // Se llama cada ~5 ms con el resultado de la deteccion (0 = sin voz)
    void updateControl (double f0)
    {
        if (f0 > 0.0)
        {
            ++voicedCnt; unvoicedCnt = 0;
            hist[histPos] = f0;
            histPos = (histPos + 1) % kHist;
            histCount = std::min (histCount + 1, kHist);
        }
        else
        {
            ++unvoicedCnt; voicedCnt = 0;
        }

        if (! voiced && voicedCnt >= 2)
        {
            voiced = true;
            corr = 0.0;
            hasTarget = false;
        }
        if (voiced && unvoicedCnt >= 3)
        {
            voiced = false;
            histCount = 0; histPos = 0;
        }

        if (! voiced || f0 <= 0.0 || histCount == 0) return;

        // mediana de las ultimas detecciones para evitar saltos de octava
        double f;
        if (histCount >= kHist)
            f = std::max (std::min (hist[0], hist[1]), std::min (std::max (hist[0], hist[1]), hist[2]));
        else
            f = hist[(histPos + kHist - 1) % kHist]; // aun pocas muestras: la mas reciente

        const double midiIn = 69.0 + 12.0 * std::log2 (f / 440.0);

        const int nearest = nearestAllowed (midiIn);
        if (hasTarget && target != nearest && isAllowed (target)
            && std::abs (midiIn - (double) target) < std::abs (midiIn - (double) nearest) + 0.25)
        {
            // histeresis: nos quedamos en la nota actual
        }
        else
        {
            target = nearest;
        }
        hasTarget = true;

        const double desired = (double) target - midiIn;
        const double hopSec = (double) hopSamples / fs;
        const double alpha = settings.retuneMs <= 0.5f
                               ? 1.0
                               : 1.0 - std::exp (-hopSec / ((double) settings.retuneMs * 0.001));
        corr += alpha * (desired - corr);

        curT0 = std::clamp (fs / f, fs / kMaxF0, fs / kMinF0);
    }

    // Coloca un grano PSOLA centrado en sNext
    void placeGrain (long long j, int chans)
    {
        const double T0 = curT0;

        // objetivo de desplazamiento (semitonos) = correccion + vibrato
        double desired = 0.0;
        if (voiced)
        {
            desired = corr;
            if (settings.vibrato)
                desired += (double) settings.vibDepthCents * 0.01 * std::sin (vibPhase);
        }
        desired = std::clamp (desired, -12.0, 12.0);

        double g = 1.0;
        if (settings.retuneMs >= 1.0f)
            g = 1.0 - std::exp (-(T0 / fs) / 0.002);
        shiftSm += g * (desired - shiftSm);

        const double ratio = std::exp2 (shiftSm / 12.0);
        const double Ts = std::max (2.0, T0 / ratio);

        vibPhase += 2.0 * kPi * (double) settings.vibRateHz * (Ts / fs);
        if (vibPhase > 2.0 * kPi) vibPhase -= 2.0 * kPi;

        // centro de analisis: avanza en multiplos exactos del periodo
        const double e = sNext - cPrev;
        const double nPer = std::max (0.0, std::floor (e / T0 + 0.1));
        double c = cPrev + nPer * T0;
        const double maxT0 = fs / kMinF0;
        if (c < sNext - 2.0 * maxT0 || c > sNext + maxT0) c = sNext;
        const double cMax = (double) (inIdx - 1) - T0 - 3.0;
        if (c > cMax) c = cMax;

        long long mMin = (long long) std::ceil (sNext - T0);
        const long long mMax = (long long) std::floor (sNext + T0);
        if (mMin < j) mMin = j;

        for (long long m = mMin; m <= mMax; ++m)
        {
            const double dt = (double) m - sNext;
            const double w = 0.5 * (1.0 + std::cos (kPi * dt / T0));
            const double pos = c + dt;
            const size_t idx = (size_t) (m & mask);
            for (int ch = 0; ch < chans; ++ch)
                acc[ch][idx] += (float) (w * hermite (inRing[ch].data(), mask, pos));
            wsum[idx] += (float) w;
        }

        cPrev = c;
        sNext += Ts;
    }

    //--------------------------------------------------------------------------
    double fs = 44100.0;
    int nCh = 2, latency = 0, hopSamples = 220, hopCount = 0;
    long long ringSize = 4096, mask = 4095, inIdx = 0;

    std::vector<float> inRing[kMaxChannels], acc[kMaxChannels], wsum;
    PitchDetector det;
    Settings settings;
    bool allowed[12] = {};

    double sNext = 0.0, cPrev = 0.0, curT0 = 350.0;
    bool voiced = false, hasTarget = false;
    int voicedCnt = 0, unvoicedCnt = 0, target = 0;
    static constexpr int kHist = 3;
    double hist[kHist] = {};
    int histCount = 0, histPos = 0;
    double corr = 0.0, shiftSm = 0.0, vibPhase = 0.0;
};

} // namespace jelly
