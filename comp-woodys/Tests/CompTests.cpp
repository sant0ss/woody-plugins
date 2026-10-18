// Numeric checks of the compressor's DSP, run from the command line (no audio device needed).
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <random>
#include <tuple>
#include <vector>

#include "CompEngine.h"
#include "Compressor.h"
#include "Crossover.h"

using namespace compwoody;

namespace
{
int failures = 0;
int checks = 0;
constexpr double pi = 3.14159265358979323846;
constexpr double sr = 48000.0;

void check (bool ok, const char* what, double value = 0.0, double expected = 0.0)
{
    ++checks;
    std::printf ("%s  %s", ok ? "PASS" : "FAIL", what);
    if (! ok)
        std::printf ("   (got %.4f, expected %.4f)", value, expected);
    std::printf ("\n");
    failures += ok ? 0 : 1;
}

void checkNear (double value, double expected, double tolerance, const char* what)
{
    check (std::abs (value - expected) <= tolerance, what, value, expected);
}

double toDb (double linear) { return 20.0 * std::log10 (std::max (1.0e-9, linear)); }
double fromDb (double db) { return std::pow (10.0, db / 20.0); }

std::vector<float> sine (double freq, double amplitude, int n, int offset = 0)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * freq * (i + offset) / sr));
    return x;
}

std::vector<float> noise (int n, float amplitude, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    std::vector<float> x ((size_t) n);
    for (auto& v : x)
        v = dist (rng);
    return x;
}

double peakOf (const std::vector<float>& x, size_t from)
{
    double p = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        p = std::max (p, (double) std::abs (x[i]));
    return p;
}

double rms (const std::vector<float>& x, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) (x.size() - from));
}

// Level of one frequency component (single-bin DFT)
double bin (const std::vector<float>& x, double freq, size_t from)
{
    std::complex<double> sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * std::polar (1.0, -2.0 * pi * freq * (double) i / sr);
    return 2.0 * std::abs (sum) / (double) (x.size() - from);
}

Compressor::Settings basic (double thresholdDb = -20.0, double ratio = 4.0, double kneeDb = 0.0)
{
    Compressor::Settings s;
    s.thresholdDb = (float) thresholdDb;
    s.ratio = (float) ratio;
    s.kneeDb = (float) kneeDb;
    s.attackMs = 1.0f;
    s.releaseMs = 100.0f;
    return s;
}

struct Result
{
    std::vector<float> left, right;
    std::vector<float> gr; // meter after each small chunk
};

Result run (const Compressor::Settings& s, std::vector<float> left, std::vector<float> right, int chunk = 8)
{
    Compressor c;
    c.prepare (sr, 512);
    c.setSettings (s);

    Result r;
    r.left = std::move (left);
    r.right = std::move (right);

    for (size_t start = 0; start < r.left.size(); start += (size_t) chunk)
    {
        const int n = (int) std::min<size_t> ((size_t) chunk, r.left.size() - start);
        c.process (r.left.data() + start, r.right.data() + start, n);
        r.gr.push_back (c.getGainReductionDb());
    }

    return r;
}

double grAt (const Result& r, double seconds, int chunk = 8) { return r.gr[(size_t) (seconds * sr / chunk)]; }

// Gain reduction a steady sine ends up with, from the level change at the output
double measuredReduction (const Compressor::Settings& s, double levelDb, double freq = 2000.0)
{
    const int n = (int) sr;
    auto in = sine (freq, fromDb (levelDb), n);
    const auto r = run (s, in, in, 512);
    return levelDb - toDb (peakOf (r.left, (size_t) (n * 0.8)));
}

double expectedReduction (double levelDb, double thr, double ratio, double knee)
{
    return gainReductionDb ((float) levelDb, (float) thr, inverseRatio ((float) ratio), (float) knee);
}

void staticCurveTests()
{
    std::printf ("-- static curve\n");

    for (double level : { -40.0, -30.0, -20.0, -10.0, 0.0 })
    {
        const auto s = basic (-20.0, 4.0, 0.0);
        const double expected = expectedReduction (level, -20.0, 4.0, 0.0);
        const double got = measuredReduction (s, level);
        char name[96];
        std::snprintf (name, sizeof name, "hard knee, -20 dB / 4:1, input %.0f dB: reduction %.2f dB", level, expected);
        checkNear (got, expected, 0.2, name);
    }

    for (double level : { -26.0, -23.0, -20.0, -17.0, -14.0, -6.0 })
    {
        const auto s = basic (-20.0, 4.0, 12.0);
        const double expected = expectedReduction (level, -20.0, 4.0, 12.0);
        const double got = measuredReduction (s, level);
        char name[96];
        std::snprintf (name, sizeof name, "12 dB knee, -20 dB / 4:1, input %.0f dB: reduction %.2f dB", level, expected);
        checkNear (got, expected, 0.2, name);
    }

    check (gainReductionDb (-20.0f, -20.0f, 0.25f, 12.0f) > 0.9f && gainReductionDb (-20.0f, -20.0f, 0.25f, 12.0f) < 1.3f,
           "soft knee gives a smooth reduction at the threshold");
    check (std::abs (gainReductionDb (-14.0f, -20.0f, 0.25f, 12.0f) - gainReductionDb (-13.99f, -20.0f, 0.25f, 12.0f)) < 0.05f,
           "knee joins the straight part without a step");

    {
        const auto s = basic (-20.0, limitRatio, 0.0);
        checkNear (measuredReduction (s, -6.0), 14.0, 0.3, "limit setting holds the level at the threshold");
    }

    {
        auto s = basic (-20.0, 4.0, 0.0);
        s.makeupDb = 6.0f;
        const double got = measuredReduction (s, -10.0);
        checkNear (got, 7.5 - 6.0, 0.2, "manual makeup adds gain after the reduction");
    }

    {
        auto s = basic (-20.0, 4.0, 0.0);
        s.autoMakeup = true;
        const double expectedMakeup = 20.0 * 0.75;
        checkNear (autoMakeupDb (-20.0f, 0.25f), expectedMakeup, 0.01, "auto makeup formula");
        const double got = measuredReduction (s, -10.0);
        checkNear (got, 7.5 - expectedMakeup, 0.2, "auto makeup is applied");
    }
}

void ballisticsTests()
{
    std::printf ("-- attack and release\n");

    {
        auto s = basic (-20.0, 4.0, 0.0);
        s.attackMs = 20.0f;
        s.releaseMs = 500.0f;
        auto in = sine (3000.0, fromDb (-10.0), (int) (sr * 0.5));
        const auto r = run (s, in, in);
        const double target = 7.5;
        checkNear (grAt (r, 0.020), target * (1.0 - std::exp (-1.0)), 0.3, "gain reduction reaches 63% after one attack time constant");
        checkNear (grAt (r, 0.040), target * (1.0 - std::exp (-2.0)), 0.3, "86% after two attack time constants");
        checkNear (grAt (r, 0.45), target, 0.15, "and settles at the target");
    }

    {
        auto s = basic (-20.0, 4.0, 0.0);
        s.attackMs = 1.0f;
        s.releaseMs = 200.0f;
        auto in = sine (3000.0, fromDb (-10.0), (int) sr);
        auto quiet = sine (3000.0, fromDb (-50.0), (int) sr, (int) sr);
        in.insert (in.end(), quiet.begin(), quiet.end());
        const auto r = run (s, in, in);
        const double start = grAt (r, 0.99);
        const double threshold = start * std::exp (-1.0);

        double t = 1.0;
        while (t < 1.8 && grAt (r, t) > threshold)
            t += 1.0 / 1000.0;
        const double releaseTime = (t - 1.0) * 1000.0;
        checkNear (releaseTime, 200.0, 25.0, "gain reduction falls to 37% after about one release time constant (ms)");
        check (grAt (r, 1.9) < 0.5, "and recovers fully");
    }

    {
        auto s = basic (-20.0, 4.0, 0.0);
        s.releaseMs = 100.0f;
        auto fast = s;
        fast.attackMs = 0.1f;
        auto slow = s;
        slow.attackMs = 50.0f;
        auto in = sine (3000.0, fromDb (-6.0), 4800);
        const double fastGr = grAt (run (fast, in, in), 0.005);
        const double slowGr = grAt (run (slow, in, in), 0.005);
        check (fastGr > slowGr * 3.0, "a short attack reacts much faster than a long one", fastGr, slowGr * 3.0);
    }

    {
        // Auto release: a short burst lets go quickly, a long loud passage slowly
        auto s = basic (-20.0, 4.0, 0.0);
        s.autoRelease = true;
        auto releaseTimeAfter = [&] (double loudSeconds)
        {
            auto in = sine (3000.0, fromDb (-8.0), (int) (sr * loudSeconds));
            auto quiet = sine (3000.0, fromDb (-60.0), (int) (sr * 4.0), (int) (sr * loudSeconds));
            in.insert (in.end(), quiet.begin(), quiet.end());
            const auto r = run (s, in, in);
            const double begin = grAt (r, loudSeconds - 0.001);
            double t = loudSeconds;
            while (t < loudSeconds + 3.9 && grAt (r, t) > begin * 0.5)
                t += 1.0 / 1000.0;
            return t - loudSeconds;
        };

        const double shortBurst = releaseTimeAfter (0.1);
        const double sustained = releaseTimeAfter (3.0);
        std::printf ("      auto release half-time: burst %.0f ms, sustained %.0f ms\n", shortBurst * 1000.0, sustained * 1000.0);
        check (shortBurst < 0.2, "auto release after a short burst is fast", shortBurst, 0.2);
        check (sustained > 0.4, "auto release after sustained compression is slow", sustained, 0.4);
        check (sustained > shortBurst * 3.0, "auto release is program dependent", sustained, shortBurst * 3.0);
    }
}

void behaviourTests()
{
    std::printf ("-- behaviour\n");

    {
        auto s = basic (-60.0, 1.0, 12.0);
        auto in = noise (48000, 0.8f);
        const auto r = run (s, in, in, 512);
        double worst = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
            worst = std::max (worst, (double) std::abs (r.left[i] - in[i]));
        check (worst < 1.0e-6, "ratio 1:1 is transparent", worst, 0.0);
    }

    {
        auto s = basic (-20.0, 4.0, 0.0);
        const int n = (int) sr;
        const auto loud = sine (2000.0, fromDb (-10.0), n);
        const auto quiet = sine (2000.0, fromDb (-40.0), n);

        auto reductionOfQuietChannel = [&] (float link)
        {
            s.link = link;
            const auto r = run (s, loud, quiet, 512);
            return std::make_pair (-toDb (peakOf (r.left, (size_t) (n * 0.8)) / fromDb (-10.0)),
                                   -toDb (peakOf (r.right, (size_t) (n * 0.8)) / fromDb (-40.0)));
        };

        const auto unlinked = reductionOfQuietChannel (0.0f);
        const auto half = reductionOfQuietChannel (0.5f);
        const auto linked = reductionOfQuietChannel (1.0f);
        checkNear (unlinked.first, 7.5, 0.25, "link 0%: the loud channel is reduced");
        checkNear (unlinked.second, 0.0, 0.05, "link 0%: the quiet channel is left alone");
        checkNear (half.second, 3.75, 0.25, "link 50%: the quiet channel gets half the reduction");
        checkNear (linked.second, 7.5, 0.25, "link 100%: both channels get the same reduction");
        checkNear (linked.first, linked.second, 0.05, "link 100%: identical gain on both channels");
    }

    {
        const int n = (int) sr;
        auto in = sine (50.0, fromDb (-10.0), n);
        auto s = basic (-20.0, 4.0, 0.0);
        auto finalGr = [&] (float hpf)
        {
            s.scHighPassHz = hpf;
            return run (s, in, in, 4800).gr.back();
        };

        const double open = finalGr (20.0f);
        const double filtered = finalGr (500.0f);
        check (open > 5.0, "a 50 Hz tone triggers the compressor with the sidechain filter off", open, 5.0);
        check (filtered < 0.2, "the sidechain high-pass at 500 Hz ignores it", filtered, 0.2);

        // The audio itself is not filtered
        s.scHighPassHz = 500.0f;
        s.ratio = 1.0f;
        const auto r = run (s, in, in, 512);
        check (peakOf (r.left, (size_t) (n * 0.5)) > fromDb (-10.5), "the sidechain filter does not touch the audio path");
    }

    {
        const int n = 4800;
        std::vector<float> burst (n, 0.0f);
        const auto tone = sine (2000.0, fromDb (-6.0), 96);
        std::copy (tone.begin(), tone.end(), burst.begin() + 2400);

        auto s = basic (-30.0, 10.0, 0.0);
        s.attackMs = 0.1f;
        auto peakSettings = s;
        auto rmsSettings = s;
        rmsSettings.rms = true;

        auto maxGr = [&] (const Compressor::Settings& settings)
        {
            const auto r = run (settings, burst, burst, 4);
            return *std::max_element (r.gr.begin(), r.gr.end());
        };

        const double peakGr = maxGr (peakSettings);
        const double rmsGr = maxGr (rmsSettings);
        check (peakGr > rmsGr + 5.0, "RMS detector reacts less than peak to a 2 ms burst", peakGr, rmsGr + 5.0);

        auto steadyRms = basic (-20.0, 4.0, 0.0);
        steadyRms.rms = true;
        checkNear (measuredReduction (steadyRms, -10.0), 7.5, 0.3, "RMS detector reads a steady sine like the peak detector");
    }

    {
        auto s = basic (-20.0, 4.0, 6.0);
        s.mix = 0.0f;
        auto in = noise (24000, 0.9f);
        const auto r = run (s, in, in, 512);
        double worst = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
            worst = std::max (worst, (double) std::abs (r.left[i] - in[i]));
        check (worst < 1.0e-6, "mix 0% returns the dry signal", worst, 0.0);
    }

    {
        // Denormals and extreme settings stay finite
        auto s = basic (-60.0, limitRatio, 24.0);
        s.attackMs = 0.1f;
        s.releaseMs = 5.0f;
        s.autoMakeup = true;
        s.makeupDb = 24.0f;
        auto in = noise (48000, 1.0f);
        std::fill (in.begin() + 20000, in.end(), 0.0f);
        const auto r = run (s, in, in, 512);
        bool finite = true;
        for (float v : r.left)
            finite = finite && std::isfinite (v);
        check (finite, "extreme settings produce finite output");
    }
}

EngineParams neutralSingle()
{
    EngineParams p;
    p.single.ratio = 1.0f;
    p.single.makeupDb = 0.0f;
    p.single.mix = 1.0f;
    return p;
}

void engineTests()
{
    std::printf("-- lookahead, mix, mid/side, block sizes\n");

    for (int choice = 0; choice < (int) lookaheadMs.size(); ++choice)
    {
        CompEngine engine;
        engine.prepare (sr, 256);
        auto p = neutralSingle();
        p.lookaheadChoice = choice;
        p.single.thresholdDb = -30.0f;
        p.single.ratio = 1.0f;

        std::vector<float> left (4096, 0.0f), right (4096, 0.0f);
        left[300] = 0.5f;
        right[300] = 0.5f;
        engine.process (left.data(), right.data(), (int) left.size(), p);

        const auto peak = (int) (std::max_element (left.begin(), left.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - left.begin());
        const int latency = CompEngine::latencyFor (p, sr);
        char name[96];
        std::snprintf (name, sizeof name, "lookahead %d ms: impulse delayed by exactly the reported %d samples", lookaheadMs[(size_t) choice], latency);
        check (peak - 300 == latency && std::abs (left[(size_t) peak] - 0.5f) < 1.0e-6f && right[(size_t) peak] == left[(size_t) peak], name, peak - 300, latency);
    }

    {
        const int expected = (int) std::lround (0.005 * sr);
        auto p = neutralSingle();
        p.lookaheadChoice = 3;
        check (CompEngine::latencyFor (p, sr) == expected, "5 ms lookahead is 240 samples at 48 kHz", CompEngine::latencyFor (p, sr), expected);
        p.multiband = true;
        check (CompEngine::latencyFor (p, sr) == 0, "multiband mode adds no latency");
    }

    {
        // Heavy compression with the lookahead on: mix 0 is the delayed dry signal
        auto in = noise (20000, 0.8f);
        auto p = neutralSingle();
        p.lookaheadChoice = 2;
        p.single.thresholdDb = -40.0f;
        p.single.ratio = 10.0f;
        const int latency = CompEngine::latencyFor (p, sr);

        for (bool midSide : { false, true })
        {
            p.midSide = midSide;
            p.single.mix = 0.0f;
            CompEngine engine;
            engine.prepare (sr, 512);
            auto left = in, right = in;
            std::reverse (right.begin(), right.end());
            auto dryRight = right;
            engine.process (left.data(), right.data(), (int) left.size(), p);

            double worst = 0.0;
            for (size_t i = (size_t) latency; i < in.size(); ++i)
                worst = std::max (worst, (double) std::abs (left[i] - in[i - (size_t) latency]) + std::abs (right[i] - dryRight[i - (size_t) latency]));
            check (worst < 2.0e-6, midSide ? "mix 0% in mid/side returns the delay-matched dry signal"
                                           : "mix 0% returns the delay-matched dry signal", worst, 0.0);
        }

        // 50% is the average of dry and wet, sample by sample
        auto render = [&] (float mix)
        {
            p.midSide = false;
            p.single.mix = mix;
            CompEngine engine;
            engine.prepare (sr, 512);
            auto left = in, right = in;
            engine.process (left.data(), right.data(), (int) left.size(), p);
            return left;
        };

        const auto dry = render (0.0f), wet = render (1.0f), half = render (0.5f);
        double worst = 0.0;
        for (size_t i = 0; i < dry.size(); ++i)
            worst = std::max (worst, (double) std::abs (half[i] - 0.5f * (dry[i] + wet[i])));
        check (worst < 1.0e-6, "mix 50% is the average of the dry and wet paths", worst, 0.0);
        check (rms (wet) < 0.6 * rms (dry), "the wet path is really compressed");
    }

    {
        auto p = neutralSingle();
        p.bypass = true;
        p.lookaheadChoice = 1;
        p.single.thresholdDb = -50.0f;
        p.single.ratio = 20.0f;
        const int latency = CompEngine::latencyFor (p, sr);
        const auto in = noise (8000, 0.7f);
        auto left = in, right = in;
        CompEngine engine;
        engine.prepare (sr, 512);
        engine.process (left.data(), right.data(), (int) in.size(), p);
        double worst = 0.0;
        for (size_t i = (size_t) latency; i < in.size(); ++i)
            worst = std::max (worst, (double) std::abs (left[i] - in[i - (size_t) latency]));
        check (worst < 1.0e-6, "bypass passes the audio with the same delay", worst, 0.0);
    }

    {
        const int n = (int) sr;
        const double midAmp = 0.02, sideAmp = 0.5;
        const auto m = sine (700.0, midAmp, n), s = sine (2000.0, sideAmp, n);

        auto render = [&] (bool midSide)
        {
            std::vector<float> left ((size_t) n), right ((size_t) n);
            for (size_t i = 0; i < left.size(); ++i)
            {
                left[i] = m[i] + s[i];
                right[i] = m[i] - s[i];
            }

            auto p = neutralSingle();
            p.midSide = midSide;
            p.single.thresholdDb = -20.0f;
            p.single.ratio = 4.0f;
            p.single.kneeDb = 0.0f;
            p.single.attackMs = 1.0f;
            p.single.link = 0.0f;
            CompEngine engine;
            engine.prepare (sr, 512);
            engine.process (left.data(), right.data(), n, p);

            std::vector<float> mid ((size_t) n), side ((size_t) n);
            for (size_t i = 0; i < mid.size(); ++i)
            {
                mid[i] = 0.5f * (left[i] + right[i]);
                side[i] = 0.5f * (left[i] - right[i]);
            }
            return std::make_pair (toDb (bin (mid, 700.0, (size_t) n / 2) / midAmp), toDb (bin (side, 2000.0, (size_t) n / 2) / sideAmp));
        };

        const auto ms = render (true);
        const auto lr = render (false);
        checkNear (ms.second, -10.5, 0.4, "mid/side: the loud side signal is compressed");
        checkNear (ms.first, 0.0, 0.05, "mid/side: the quiet mid signal is untouched");
        check (lr.first < -3.0, "stereo mode: the same signal does pull the mid down", lr.first, -3.0);
    }

    {
        // Whatever the block size, the result is the same (blocks longer than prepare are chunked)
        auto in = noise (5000, 0.8f, 7);
        for (bool multiband : { false, true })
        {
            auto p = neutralSingle();
            p.multiband = multiband;
            p.single.thresholdDb = -25.0f;
            p.single.ratio = 6.0f;
            p.lookaheadChoice = 2;
            for (auto& b : p.bands)
            {
                b.thresholdDb = -30.0f;
                b.ratio = 6.0f;
            }

            CompEngine whole, pieces;
            whole.prepare (sr, 512);
            pieces.prepare (sr, 512);

            auto a = in, aRight = in;
            whole.process (a.data(), aRight.data(), (int) a.size(), p);

            auto b = in, bRight = in;
            for (size_t start = 0; start < b.size(); start += 97)
            {
                const int count = (int) std::min<size_t> (97, b.size() - start);
                pieces.process (b.data() + start, bRight.data() + start, count, p);
            }

            double worst = 0.0;
            for (size_t i = 0; i < a.size(); ++i)
                worst = std::max (worst, (double) std::abs (a[i] - b[i]));
            check (worst < 1.0e-5, multiband ? "multiband: one 5000-sample call equals many small blocks"
                                             : "single band: one 5000-sample call equals many small blocks", worst, 0.0);
            check (peakOf (a, 0) < 2.0, multiband ? "multiband: long block output is sane" : "single band: long block output is sane");
        }
    }
}

// Impulse response of every band at the default crossovers
struct BandResponses
{
    std::array<std::vector<float>, numBands> ir;
};

BandResponses measureBands (const std::array<float, numCrossovers>& freqs, int length)
{
    BandSplitter splitter;
    splitter.prepare (sr);
    splitter.setCrossovers (freqs, true);

    std::vector<float> in ((size_t) length, 0.0f);
    in[0] = 1.0f;
    std::array<std::array<std::vector<float>, numBands>, 2> out;
    float* pointers[2][numBands];
    for (size_t c = 0; c < 2; ++c)
        for (size_t b = 0; b < numBands; ++b)
        {
            out[c][b].assign ((size_t) length, 0.0f);
            pointers[c][b] = out[c][b].data();
        }

    float* const* const bands[2] = { pointers[0], pointers[1] };
    const float* const input[2] = { in.data(), in.data() };
    splitter.split (input, bands, length);

    BandResponses r;
    for (size_t b = 0; b < numBands; ++b)
        r.ir[b] = out[0][b];
    return r;
}

std::complex<double> response (const std::vector<float>& ir, double freq)
{
    std::complex<double> sum = 0.0, phasor = 1.0;
    const auto step = std::polar (1.0, -2.0 * pi * freq / sr);
    for (float v : ir)
    {
        sum += (double) v * phasor;
        phasor *= step;
    }
    return sum;
}

void multibandTests()
{
    std::printf ("-- multiband\n");

    {
        const auto o = orderedCrossovers ({ 3000.0f, 500.0f, 100.0f }, sr);
        check (o[1] >= o[0] * 1.49f && o[2] >= o[1] * 1.49f && o[2] <= 20000.0f, "crossover frequencies are kept ordered with a minimum spacing");
        const auto fine = orderedCrossovers ({ 120.0f, 1000.0f, 5000.0f }, sr);
        check (fine[0] == 120.0f && fine[1] == 1000.0f && fine[2] == 5000.0f, "well spaced crossovers are left alone");
    }

    const std::array<float, numCrossovers> freqs { 120.0f, 1000.0f, 5000.0f };
    const auto bands = measureBands (freqs, 65536);

    auto sumAt = [&] (double f)
    {
        std::complex<double> sum = 0.0;
        for (const auto& ir : bands.ir)
            sum += response (ir, f);
        return sum;
    };

    double worst = 0.0;
    for (int i = 0; i <= 150; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 150.0);
        worst = std::max (worst, std::abs (toDb (std::abs (sumAt (f)))));
    }
    check (worst < 0.1, "bands sum to a flat magnitude within 0.1 dB from 20 Hz to 20 kHz", worst, 0.1);

    for (int i = 0; i < numCrossovers; ++i)
    {
        const double f = freqs[(size_t) i];
        const auto low = response (bands.ir[(size_t) i], f);
        const auto high = response (bands.ir[(size_t) i + 1], f);
        char name[96];
        std::snprintf (name, sizeof name, "crossover %d (%.0f Hz): both neighbours are 6 dB down", i + 1, f);
        check (std::abs (toDb (std::abs (low)) + 6.02) < 0.3 && std::abs (toDb (std::abs (high)) + 6.02) < 0.3, name, toDb (std::abs (low)), -6.02);
        std::snprintf (name, sizeof name, "crossover %d: neighbouring bands are in phase (no cancellation)", i + 1);
        check (std::abs (low + high) > 0.97 * (std::abs (low) + std::abs (high)), name, std::abs (low + high), std::abs (low) + std::abs (high));
        std::snprintf (name, sizeof name, "crossover %d: sum is 0 dB", i + 1);
        checkNear (toDb (std::abs (sumAt (f))), 0.0, 0.1, name);
    }

    check (toDb (std::abs (response (bands.ir[0], 20000.0))) < -60.0, "the lowest band rejects the top of the spectrum");
    check (toDb (std::abs (response (bands.ir[3], 20.0))) < -80.0, "the highest band rejects the bottom of the spectrum");

    {
        // The bands sum to a 6th-order all-pass: with every band neutral the engine equals that all-pass on its own
        const int n = (int) sr;
        const auto in = noise (n, 0.4f, 3);

        auto reference = [&] (double f, std::vector<double>& x)
        {
            const double w0 = 2.0 * pi * f / sr, cosW = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678118654752);
            const double a0 = 1.0 + alpha, b0 = (1.0 - alpha) / a0, b1 = -2.0 * cosW / a0, b2 = (1.0 + alpha) / a0, a1 = b1, a2 = (1.0 - alpha) / a0;
            double z1 = 0.0, z2 = 0.0;
            for (auto& v : x)
            {
                const double y = b0 * v + z1;
                z1 = b1 * v - a1 * y + z2;
                z2 = b2 * v - a2 * y;
                v = y;
            }
        };

        std::vector<double> ref (in.begin(), in.end());
        for (double f : { 120.0, 1000.0, 5000.0 })
            reference (f, ref);

        EngineParams p;
        p.multiband = true;
        for (auto& b : p.bands)
        {
            b.ratio = 1.0f;
            b.makeupDb = 0.0f;
        }
        CompEngine engine;
        engine.prepare (sr, 512);
        auto left = in, right = in;
        engine.process (left.data(), right.data(), n, p);

        double errorEnergy = 0.0, refEnergy = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            errorEnergy += (left[i] - ref[i]) * (left[i] - ref[i]);
            refEnergy += ref[i] * ref[i];
        }
        const double nullDb = 10.0 * std::log10 (errorEnergy / refEnergy);
        std::printf ("      neutral multiband vs matched all-pass: %.1f dB\n", nullDb);
        check (nullDb < -80.0, "neutral multiband nulls against the all-pass-matched input below -80 dB", nullDb, -80.0);

        // And its magnitude is the input's
        check (std::abs (toDb (rms (left)) - toDb (rms (in))) < 0.1, "neutral multiband keeps the level of white noise");
        check (left == right, "both channels are processed identically");
    }

    {
        const int n = (int) sr * 2;
        const double lowAmp = 0.5, highAmp = 0.25;
        auto low = sine (40.0, lowAmp, n), high = sine (10000.0, highAmp, n);
        std::vector<float> in (low.size());
        for (size_t i = 0; i < in.size(); ++i)
            in[i] = low[i] + high[i];

        auto render = [&] (int compressedBand, bool solo = false, bool bypass = false)
        {
            EngineParams p;
            p.multiband = true;
            for (int b = 0; b < numBands; ++b)
            {
                p.bands[(size_t) b].ratio = 1.0f;
                if (b == compressedBand)
                {
                    p.bands[(size_t) b].thresholdDb = -40.0f;
                    p.bands[(size_t) b].ratio = 20.0f;
                    p.bands[(size_t) b].attackMs = 1.0f;
                    p.bands[(size_t) b].releaseMs = 50.0f;
                    p.bands[(size_t) b].solo = solo;
                    p.bands[(size_t) b].bypass = bypass;
                }
            }
            CompEngine engine;
            engine.prepare (sr, 512);
            auto left = in, right = in;
            engine.process (left.data(), right.data(), n, p);
            std::array<float, numBands> grs;
            for (int b = 0; b < numBands; ++b)
                grs[(size_t) b] = engine.getBandGainReductionDb (b);
            return std::make_tuple (left, grs);
        };

        const auto [neutral, neutralGr] = render (-1);
        const size_t from = (size_t) n / 2;
        const double lowNeutral = bin (neutral, 40.0, from), highNeutral = bin (neutral, 10000.0, from);

        const auto [lowCompressed, lowGr] = render (0);
        check (toDb (bin (lowCompressed, 40.0, from) / lowNeutral) < -15.0, "compressing band 1 pulls its content down",
               toDb (bin (lowCompressed, 40.0, from) / lowNeutral), -15.0);
        checkNear (toDb (bin (lowCompressed, 10000.0, from) / highNeutral), 0.0, 0.1, "compressing band 1 leaves band 4 unchanged");
        check (lowGr[0] > 15.0 && lowGr[1] < 0.01f && lowGr[2] < 0.01f && lowGr[3] < 0.01f, "only band 1 shows gain reduction", lowGr[0], 15.0);

        const auto [highCompressed, highGr] = render (3);
        check (toDb (bin (highCompressed, 10000.0, from) / highNeutral) < -15.0, "compressing band 4 pulls its content down",
               toDb (bin (highCompressed, 10000.0, from) / highNeutral), -15.0);
        checkNear (toDb (bin (highCompressed, 40.0, from) / lowNeutral), 0.0, 0.1, "compressing band 4 leaves band 1 unchanged");
        check (highGr[3] > 15.0 && highGr[0] < 0.01f, "only band 4 shows gain reduction", highGr[3], 15.0);

        const auto [bypassed, bypassedGr] = render (0, false, true);
        double worst = 0.0;
        for (size_t i = 0; i < neutral.size(); ++i)
            worst = std::max (worst, (double) std::abs (bypassed[i] - neutral[i]));
        check (worst < 1.0e-6 && bypassedGr[0] == 0.0f, "a bypassed band is left neutral and shows no reduction", worst, 0.0);

        const auto [soloed, soloGr] = render (0, true);
        check (toDb (bin (soloed, 10000.0, from) / highNeutral) < -50.0, "solo on band 1 silences the high band", toDb (bin (soloed, 10000.0, from) / highNeutral), -50.0);
        check (bin (soloed, 40.0, from) > 0.0 && soloGr[0] > 15.0, "solo keeps the soloed band audible");
    }

    {
        // Multiband bypass hands back the input itself
        const auto in = noise (8000, 0.5f, 5);
        EngineParams p;
        p.multiband = true;
        p.bypass = true;
        for (auto& b : p.bands)
            b.thresholdDb = -50.0f;
        CompEngine engine;
        engine.prepare (sr, 512);
        auto left = in, right = in;
        engine.process (left.data(), right.data(), (int) in.size(), p);
        check (left == in, "multiband bypass is the untouched input");
    }

    {
        // Moving a crossover while audio runs stays smooth
        EngineParams p;
        p.multiband = true;
        for (auto& b : p.bands)
            b.ratio = 1.0f;
        CompEngine engine;
        engine.prepare (sr, 512);

        const int n = (int) sr;
        auto in = sine (800.0, 0.5, n);
        auto right = in;
        double biggestJump = 0.0;
        float previous = 0.0f;
        for (int start = 0; start < n; start += 480)
        {
            p.crossovers[1] = 600.0f + 800.0f * (float) start / (float) n;
            engine.process (in.data() + start, right.data() + start, 480, p);
            for (int i = 0; i < 480; ++i)
            {
                if (start > 4800)
                    biggestJump = std::max (biggestJump, (double) std::abs (in[(size_t) (start + i)] - previous));
                previous = in[(size_t) (start + i)];
            }
        }
        check (biggestJump < 0.15, "sweeping a crossover does not click", biggestJump, 0.15);
    }
}
} // namespace

int main()
{
    juce::ScopedNoDenormals noDenormals;

    staticCurveTests();
    ballisticsTests();
    behaviourTests();
    engineTests();
    multibandTests();

    std::printf ("\n%s (%d checks, %d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", checks, failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
