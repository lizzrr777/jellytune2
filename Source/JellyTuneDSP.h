// JellyTuneDSP.h
// Motor de autotune: deteccion de pitch (YIN) + correccion con PSOLA.
// Incluye: afinacion de referencia (A4), formante y rango de voz (latencia).
// Codigo C++ puro (sin JUCE) para poder probarlo por separado.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace jelly
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kMinF0 = 80.0;    // Hz, limite inferior absoluto (reserva de memoria)
constexpr double kMaxF0 = 1000.0;  // Hz, voz mas aguda que se detecta
constexpr float  kFormantMax = 8.0f; // semitonos de formante (mas alla se degrada el sonido)

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
        setMinF0 (kMinF0); // peor caso: define el tamano de las reservas

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

    // Voz mas grave que se quiere detectar. Subirla acorta la ventana de analisis.
    void setMinF0 (double f)
    {
        minF0   = std::max (f, kMinF0);
        tauMaxD = (int) std::ceil (fsd / minF0) + 1;
        Wd      = (int) std::ceil (2.75 * tauMaxD);
        Nd      = Wd + tauMaxD + 2;
        Wf      = Wd * D;
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
        if (f0 < minF0 * 0.97 || f0 > kMaxF0 * 1.03) return 0.0;
        return f0;
    }

private:
    double fs = 44100.0, fsd = 11025.0, minF0 = kMinF0;
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
        float tuningHz      = 440.0f; // afinacion de referencia (A4)
        float formantSemis  = 0.0f;   // -8..+8: cambia el timbre sin mover la nota
        int   voiceRange    = 1;      // 0 grave (80 Hz), 1 media (110 Hz), 2 aguda (150 Hz)
    };

    void prepare (double sampleRate, int numChannels)
    {
        fs  = sampleRate;
        nCh = std::clamp (numChannels, 1, kMaxChannels);

        // reservas para el peor caso (rango grave + formante alto)
        const int latencyMax = (int) std::ceil (2.1 * fs / kMinF0) + 16;
        const int need = latencyMax + (int) std::ceil (6.0 * fs / kMinF0) + 128;
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
        levelAlpha = 1.0 - std::exp (-1.0 / (0.15 * fs));
        gainAlpha  = 1.0 - std::exp (-1.0 / (0.05 * fs));
        applySettings();
        applyRange();
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
        eIn = eOut = 0.0; gComp = 1.0;
        phi = std::exp2 ((double) std::clamp (settings.formantSemis, -kFormantMax, kFormantMax) / 12.0);
        curT0 = fs * 0.008;
    }

    int getLatencySamples() const { return latency; }

    void setSettings (const Settings& s)
    {
        const bool rangeChanged = (s.voiceRange != settings.voiceRange);
        settings = s;
        applySettings();
        if (rangeChanged)
        {
            applyRange();
            reset(); // cambia la latencia: se reinician los buffers
        }
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
            double sumIn = 0.0, sumOut = 0.0;
            for (int ch = 0; ch < chans; ++ch)
            {
                const float y = acc[ch][idx] / w;
                const float xin = inRing[ch][idx];
                sumIn += (double) xin * xin;
                sumOut += (double) y * y;
                channels[ch][n] = y;
                acc[ch][idx] = 0.0f;
            }
            wsum[idx] = 0.0f;

            // Compensacion de nivel: el formante cambia la energia del espectro;
            // igualamos el nivel de salida con el de entrada (lento, sin bombeo).
            eIn  += levelAlpha * (sumIn  / chans - eIn);
            eOut += levelAlpha * (sumOut / chans - eOut);
            double gTarget = 1.0;
            if (std::abs (settings.formantSemis) > 0.05f && eOut > 1e-10)
                gTarget = std::clamp (std::sqrt (eIn / eOut), 0.35, 2.5);
            gComp += gainAlpha * (gTarget - gComp);
            if (std::abs (gComp - 1.0) > 1e-6)
                for (int ch = 0; ch < chans; ++ch) channels[ch][n] *= (float) gComp;
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

    // La voz mas grave que se detecta define la latencia: ~2.1 periodos de esa voz
    void applyRange()
    {
        static const double minF0Table[3] = { 80.0, 110.0, 150.0 };
        fMin = minF0Table[std::clamp (settings.voiceRange, 0, 2)];
        latency = (int) std::ceil (2.1 * fs / fMin) + 16;
        det.setMinF0 (fMin);
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

        // A4 = tuningHz  (440 por defecto)
        const double ref = (double) std::clamp (settings.tuningHz, 380.0f, 500.0f);
        const double midiIn = 69.0 + 12.0 * std::log2 (f / ref);

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

        curT0 = std::clamp (fs / f, fs / kMaxF0, fs / fMin);
    }

    // Coloca un grano PSOLA centrado en sNext
    void placeGrain (long long j, int chans)
    {
        const double T0 = curT0;

        // objetivo de desplazamiento de tono (semitonos) = correccion + vibrato
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

        // formante: escala el eje de tiempo DENTRO del grano (cambia el timbre,
        // no la nota, porque la nota la fija el espaciado Ts de los granos).
        // Suavizado para que mover la perilla no cause saltos.
        const double phiTarget = std::exp2 ((double) std::clamp (settings.formantSemis, -kFormantMax, kFormantMax) / 12.0);
        phi += (1.0 - std::exp (-(Ts / fs) / 0.02)) * (phiTarget - phi);
        const double bias = std::max (0.0, phi - 1.0); // el grano lee mas adelante: nos atrasamos

        vibPhase += 2.0 * kPi * (double) settings.vibRateHz * (Ts / fs);
        if (vibPhase > 2.0 * kPi) vibPhase -= 2.0 * kPi;

        // centro de analisis: avanza en multiplos exactos del periodo
        const double e = sNext - cPrev;
        const double nPer = std::max (0.0, std::floor (e / T0 + 0.1 - bias));
        double c = cPrev + nPer * T0;
        const double maxT0 = fs / fMin;
        if (c < sNext - (2.0 + bias) * maxT0 || c > sNext + maxT0) c = sNext - bias * T0;
        const double cMax = (double) (inIdx - 1) - phi * T0 - 3.0;
        if (c > cMax) c = cMax;

        long long mMin = (long long) std::ceil (sNext - T0);
        const long long mMax = (long long) std::floor (sNext + T0);
        if (mMin < j) mMin = j;

        for (long long m = mMin; m <= mMax; ++m)
        {
            const double dt = (double) m - sNext;
            const double w = 0.5 * (1.0 + std::cos (kPi * dt / T0));
            const double pos = c + dt * phi;
            const size_t idx = (size_t) (m & mask);
            for (int ch = 0; ch < chans; ++ch)
                acc[ch][idx] += (float) (w * hermite (inRing[ch].data(), mask, pos));
            wsum[idx] += (float) w;
        }

        cPrev = c;
        sNext += Ts;
    }

    //--------------------------------------------------------------------------
    double fs = 44100.0, fMin = 110.0;
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
    double corr = 0.0, shiftSm = 0.0, vibPhase = 0.0, phi = 1.0;
    double eIn = 0.0, eOut = 0.0, gComp = 1.0, levelAlpha = 0.0, gainAlpha = 0.0;
};

} // namespace jelly
