// Numeric checks of the EQ's DSP, run from the command line (no audio device needed).
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "EqEngine.h"
#include "FilterDesign.h"
#include "LinearPhase.h"

using namespace eqwoody;

namespace
{
int failures = 0;
constexpr double pi = 3.14159265358979323846;

void check (bool ok, const char* what, double value = 0.0, double expected = 0.0)
{
    std::printf ("%s  %s", ok ? "PASS" : "FAIL", what);
    if (! ok)
        std::printf ("   (got %.3f, expected %.3f)", value, expected);
    std::printf ("\n");
    failures += ok ? 0 : 1;
}

double toDb (double linear) { return 20.0 * std::log10 (std::max (1.0e-9, linear)); }

std::vector<float> sine (double freq, double amplitude, int n, double sr)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * freq * i / sr));
    return x;
}

double rms (const std::vector<float>& x, size_t from)
{
    double sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) (x.size() - from));
}

// Level of one frequency component (Goertzel-style single-bin DFT)
double bin (const std::vector<float>& x, double freq, double sr, size_t from)
{
    std::complex<double> sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * std::polar (1.0, -2.0 * pi * freq * (double) i / sr);
    return 2.0 * std::abs (sum) / (double) (x.size() - from);
}

BandSettings band (int type, double freq, double gain, double q, int place = Stereo, int slope = 1)
{
    BandSettings b;
    b.on = true;
    b.type = type;
    b.freq = (float) freq;
    b.gain = (float) gain;
    b.q = (float) q;
    b.place = place;
    b.slope = slope;
    return b;
}

// Frequency response of an impulse response, direct DFT
std::complex<double> response (const std::vector<float>& ir, double freq, double sr)
{
    std::complex<double> sum = 0.0;
    for (size_t n = 0; n < ir.size(); ++n)
        sum += (double) ir[n] * std::polar (1.0, -2.0 * pi * freq * (double) n / sr);
    return sum;
}

// Direct-form FIR of the 2x2 set, to test the matrix without the real-time convolution engine
void applyImpulses (const ImpulseSet& set, const std::vector<float>& l, const std::vector<float>& r,
                    std::vector<float>& outL, std::vector<float>& outR)
{
    const size_t n = l.size();
    outL.assign (n, 0.0f);
    outR.assign (n, 0.0f);

    for (size_t i = 0; i < n; ++i)
    {
        double yl = 0.0, yr = 0.0;
        for (size_t k = 0; k < set.ir[0].size() && k <= i; ++k)
        {
            yl += (double) set.ir[0][k] * l[i - k] + (double) set.ir[1][k] * r[i - k];
            yr += (double) set.ir[2][k] * l[i - k] + (double) set.ir[3][k] * r[i - k];
        }
        outL[i] = (float) yl;
        outR[i] = (float) yr;
    }
}
} // namespace

int main()
{
    const double sr = 48000.0;
    std::printf ("--- filter design\n");

    {
        const auto bell = designBand (band (Bell, 1000, 12, 1), sr);
        check (std::abs (toDb (magnitude (bell, 2 * pi * 1000 / sr)) - 12.0) < 0.05, "bell +12 dB reads +12 dB at its centre",
               toDb (magnitude (bell, 2 * pi * 1000 / sr)), 12.0);
        check (std::abs (toDb (magnitude (bell, 2 * pi * 20 / sr))) < 0.3, "bell leaves 20 Hz alone");

        const auto lowCut = designBand (band (LowCut, 100, 0, 1, Stereo, 3), sr); // 24 dB/oct
        check (std::abs (toDb (magnitude (lowCut, 2 * pi * 100 / sr)) + 3.0) < 0.4, "24 dB/oct low cut is -3 dB at the corner",
               toDb (magnitude (lowCut, 2 * pi * 100 / sr)), -3.0);
        check (std::abs (toDb (magnitude (lowCut, 2 * pi * 50 / sr)) + 24.0) < 2.0, "24 dB/oct low cut is about -24 dB one octave down",
               toDb (magnitude (lowCut, 2 * pi * 50 / sr)), -24.0);

        const auto steep = designBand (band (HighCut, 10000, 0, 1, Stereo, 7), sr); // 96 dB/oct
        check (steep.count == 8, "96 dB/oct is eight biquads");
        check (std::abs (toDb (magnitude (steep, 2 * pi * 5000 / sr))) < 0.5, "96 dB/oct high cut is flat an octave below");
        check (toDb (magnitude (steep, 2 * pi * 16000 / sr)) < -60.0, "96 dB/oct high cut is below -60 dB well above the corner");

        const auto odd = designBand (band (LowCut, 100, 0, 1, Stereo, 2), sr); // 18 dB/oct = first order + biquad
        check (odd.count == 2 && std::abs (toDb (magnitude (odd, 2 * pi * 100 / sr)) + 3.0) < 0.4, "18 dB/oct (odd order) is -3 dB at the corner");

        const auto notch = designBand (band (Notch, 1000, 0, 4), sr);
        check (toDb (magnitude (notch, 2 * pi * 1000 / sr)) < -40.0, "notch is deep at its centre");

        const auto tilt = designBand (band (Tilt, 1000, 6, 1), sr);
        check (toDb (magnitude (tilt, 2 * pi * 40 / sr)) > 2.0 && toDb (magnitude (tilt, 2 * pi * 16000 / sr)) < -2.0,
               "tilt boosts the lows and cuts the highs");
    }

    std::printf ("--- zero-latency engine\n");

    {
        EqEngine engine;
        engine.prepare (sr, 512);

        BandsArray bands {};
        bands[0] = band (Bell, 1000, 12, 1);

        auto l = sine (1000, 0.1, 24000, sr);
        auto r = l;
        for (size_t start = 0; start < l.size(); start += 512)
        {
            const int count = (int) std::min<size_t> (512, l.size() - start);
            engine.processMinPhase (l.data() + start, r.data() + start, count, bands);
        }
        const double gain = toDb (rms (l, 12000) / rms (sine (1000, 0.1, 24000, sr), 12000));
        check (std::abs (gain - 12.0) < 0.2, "engine applies +12 dB at 1 kHz", gain, 12.0);
    }

    {
        // Placement: a side-only band must not touch a mono signal, and must lift a pure-side one
        EqEngine engine;
        engine.prepare (sr, 512);
        BandsArray bands {};
        bands[0] = band (Bell, 1000, 12, 1, Side);

        auto run = [&] (bool sideOnly)
        {
            engine.reset();
            auto l = sine (1000, 0.1, 24000, sr);
            auto r = l;
            if (sideOnly)
                for (auto& v : r)
                    v = -v;

            for (size_t start = 0; start < l.size(); start += 512)
            {
                const int count = (int) std::min<size_t> (512, l.size() - start);
                engine.processMinPhase (l.data() + start, r.data() + start, count, bands);
            }
            return toDb (rms (l, 12000) / rms (sine (1000, 0.1, 24000, sr), 12000));
        };

        const double mono = run (false), side = run (true);
        check (std::abs (mono) < 0.2, "side band leaves a mono signal untouched", mono, 0.0);
        check (std::abs (side - 12.0) < 0.3, "side band lifts a pure-side signal by 12 dB", side, 12.0);
    }

    std::printf ("--- linear phase\n");

    {
        BandsArray bands {};
        bands[0] = band (Bell, 200, 9, 2);
        bands[1] = band (HighShelf, 6000, -6, 0.7);
        bands[2] = band (LowCut, 40, 0, 1, Stereo, 1);

        const int length = 8192;
        const auto set = buildImpulses (bands, sr, length);
        const auto& ir = set.ir[0];

        // Symmetric around the centre = linear phase
        double asymmetry = 0.0, energy = 0.0;
        for (int k = 1; k < length / 2; ++k)
        {
            asymmetry = std::max (asymmetry, (double) std::abs (ir[(size_t) (length / 2 + k)] - ir[(size_t) (length / 2 - k)]));
            energy = std::max (energy, (double) std::abs (ir[(size_t) (length / 2 + k)]));
        }
        check (asymmetry < 1.0e-6 * std::max (1.0, energy), "impulse response is symmetric about its centre (linear phase)");

        double worstMagnitude = 0.0, worstPhase = 0.0;
        for (double f : { 100.0, 200.0, 500.0, 1000.0, 3000.0, 8000.0, 15000.0 })
        {
            double target = 1.0;
            for (const auto& b : bands)
                target *= magnitude (designBand (b, sr), 2 * pi * f / sr);

            const auto h = response (ir, f, sr);
            worstMagnitude = std::max (worstMagnitude, std::abs (toDb (std::abs (h)) - toDb (target)));

            // Removing the pure delay of length/2 samples must leave a real number
            const auto aligned = h * std::polar (1.0, 2.0 * pi * f * (length / 2) / sr);
            worstPhase = std::max (worstPhase, std::abs (aligned.imag()) / std::max (1.0e-9, std::abs (aligned)));
        }
        check (worstMagnitude < 0.3, "FIR magnitude matches the target curve within 0.3 dB", worstMagnitude, 0.0);
        check (worstPhase < 0.02, "FIR phase is a pure delay of length/2 samples", worstPhase, 0.0);
        check (! set.needsCross, "stereo bands need no cross terms");
    }

    {
        // The 2x2 matrix: a side band in linear-phase mode must behave like the zero-latency one
        BandsArray bands {};
        bands[0] = band (Bell, 1000, 12, 1, Side);

        const int length = 4096;
        const auto set = buildImpulses (bands, sr, length);
        check (set.needsCross, "mid/side bands need the cross terms");

        auto run = [&] (bool sideOnly)
        {
            auto l = sine (1000, 0.1, 16000, sr);
            auto r = l;
            if (sideOnly)
                for (auto& v : r)
                    v = -v;

            std::vector<float> outL, outR;
            applyImpulses (set, l, r, outL, outR);
            return toDb (rms (outL, 8000) / rms (sine (1000, 0.1, 16000, sr), 8000));
        };

        const double mono = run (false), side = run (true);
        check (std::abs (mono) < 0.3, "linear-phase side band leaves a mono signal untouched", mono, 0.0);
        check (std::abs (side - 12.0) < 0.5, "linear-phase side band lifts a pure-side signal by 12 dB", side, 12.0);
    }

    std::printf ("--- real-time convolution engine\n");

    {
        // The same engine the plugin runs: load the impulses, then measure latency and gain with real audio
        juce::ScopedJuceInitialiser_GUI juceInit;

        BandsArray bands {};
        bands[0] = band (Bell, 1000, 12, 1);

        const int length = 4096;
        const int blockSize = 512;
        const auto set = buildImpulses (bands, sr, length);

        LinearPhaseFilter filter;
        filter.prepare (sr, blockSize);
        filter.load (set, sr);

        auto run = [&] (std::vector<float> l, std::vector<float> r)
        {
            std::vector<float> outL (l.size()), outR (r.size());
            for (size_t start = 0; start < l.size(); start += (size_t) blockSize)
            {
                std::vector<float> bl (l.begin() + (long) start, l.begin() + (long) start + blockSize);
                std::vector<float> br (r.begin() + (long) start, r.begin() + (long) start + blockSize);
                filter.process (bl.data(), br.data(), blockSize, set.needsCross);
                std::copy (bl.begin(), bl.end(), outL.begin() + (long) start);
                std::copy (br.begin(), br.end(), outR.begin() + (long) start);
            }
            return outL;
        };

        // The impulse response loads on a background thread; keep feeding silence until the filter is live
        const int total = blockSize * 80;
        int latencySamples = -1;
        for (int attempt = 0; attempt < 40 && latencySamples < 0; ++attempt)
        {
            juce::Thread::sleep (50);
            std::vector<float> impulse ((size_t) total, 0.0f), silence ((size_t) total, 0.0f);
            impulse[0] = 1.0f;
            const auto out = run (impulse, silence);

            float peak = 0.0f;
            int where = 0;
            for (size_t i = 0; i < out.size(); ++i)
                if (std::abs (out[i]) > peak)
                {
                    peak = std::abs (out[i]);
                    where = (int) i;
                }

            if (peak > 0.05f)
                latencySamples = where;
        }

        check (latencySamples == length / 2, "the engine's latency is exactly half the impulse length", latencySamples, length / 2);

        const int n = blockSize * 120;
        auto l = sine (1000, 0.1, n, sr);
        auto r = l;
        const auto out = run (l, r);
        const double gain = toDb (rms (out, (size_t) n / 2) / rms (l, (size_t) n / 2));
        check (std::abs (gain - 12.0) < 0.3, "the engine applies +12 dB at 1 kHz", gain, 12.0);

        auto away = sine (60, 0.1, n, sr);
        const auto outAway = run (away, away);
        check (std::abs (toDb (rms (outAway, (size_t) n / 2) / rms (away, (size_t) n / 2))) < 0.5, "the engine leaves 60 Hz alone", 0.0, 0.0);
    }

    std::printf ("--- band saturation\n");

    {
        EqEngine engine;
        engine.prepare (sr, 512);

        BandsArray bands {};
        bands[0] = band (Bell, 1000, 0, 1);
        bands[0].sat = 1.0f;
        bands[0].satMode = 0; // soft

        const int n = 24000;
        const auto input = sine (1000, 0.5, n, sr);
        std::vector<float> satL ((size_t) n), satR ((size_t) n);
        for (size_t start = 0; start < (size_t) n; start += 512)
        {
            const int count = (int) std::min<size_t> (512, (size_t) n - start);
            engine.processSaturation (input.data() + start, input.data() + start, count, bands, satL.data() + start, satR.data() + start);
        }

        bool finite = true;
        for (float v : satL)
            finite = finite && std::isfinite (v);

        const double third = bin (satL, 3000, sr, 12000);
        const double fifth = bin (satL, 5000, sr, 12000);
        check (finite, "saturation output is finite");
        check (third > 0.005, "soft saturation adds a 3rd harmonic", third, 0.005);
        check (fifth < third, "the 5th harmonic is weaker than the 3rd");
        check (bin (satL, 2000, sr, 12000) < third * 0.2, "a symmetric curve adds almost no even harmonics");

        // Tube is asymmetric: it must add the 2nd harmonic
        bands[0].satMode = 1;
        engine.reset();
        for (size_t start = 0; start < (size_t) n; start += 512)
        {
            const int count = (int) std::min<size_t> (512, (size_t) n - start);
            engine.processSaturation (input.data() + start, input.data() + start, count, bands, satL.data() + start, satR.data() + start);
        }
        check (bin (satL, 2000, sr, 12000) > 0.003, "tube saturation adds a 2nd harmonic", bin (satL, 2000, sr, 12000), 0.003);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
