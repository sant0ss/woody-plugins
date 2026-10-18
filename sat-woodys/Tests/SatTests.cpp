// Numeric checks of Sat Woodys' DSP, run from the command line (no audio device needed).
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "SatEngine.h"
#include "SatParams.h"

using namespace satwoody;

namespace
{
int failures = 0, total = 0;
constexpr double pi = 3.14159265358979323846;
constexpr double sr = 48000.0;

void check (bool ok, const char* what, double value = 0.0, double expected = 0.0)
{
    std::printf ("%s  %s", ok ? "PASS" : "FAIL", what);
    if (! ok)
        std::printf ("   (got %.4f, expected %.4f)", value, expected);
    std::printf ("\n");
    ++total;
    failures += ok ? 0 : 1;
}

double toDb (double linear) { return 20.0 * std::log10 (std::max (1.0e-12, linear)); }

using Signal = std::vector<float>;

Signal sine (double freq, double amplitude, int n)
{
    Signal x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * freq * i / sr));
    return x;
}

Signal noise (int n, float amplitude, unsigned seed = 1)
{
    Signal x ((size_t) n);
    for (auto& v : x)
    {
        seed = seed * 1664525u + 1013904223u;
        v = amplitude * ((float) (seed >> 8) / (float) (1 << 23) - 1.0f);
    }
    return x;
}

double rms (const Signal& x, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) (x.size() - from));
}

// Level of one frequency component
double bin (const Signal& x, double freq, size_t from)
{
    std::complex<double> sum = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        sum += (double) x[i] * std::polar (1.0, -2.0 * pi * freq * (double) i / sr);
    return 2.0 * std::abs (sum) / (double) (x.size() - from);
}

struct Result { Signal left, right; };

Result run (const Signal& l, const Signal& r, const Settings& settings, int block = 512, int prepared = 512)
{
    SatEngine engine;
    engine.prepare (sr, prepared);

    Result out { l, r };
    for (size_t start = 0; start < l.size(); start += (size_t) block)
    {
        const int count = (int) std::min ((size_t) block, l.size() - start);
        engine.process (out.left.data() + start, out.right.data() + start, count, settings);
    }
    return out;
}

Signal runMono (const Signal& x, const Settings& settings)
{
    return run (x, x, settings).left;
}

bool allFinite (const Signal& x)
{
    for (float v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}

double maxDifference (const Signal& a, const Signal& b, int shiftOfA = 0)
{
    double worst = 0.0;
    for (size_t i = 0; i < b.size(); ++i)
    {
        const long j = (long) i + shiftOfA;
        if (j < 0 || j >= (long) a.size())
            continue;
        worst = std::max (worst, (double) std::abs (a[(size_t) j] - b[i]));
    }
    return worst;
}

Settings base()
{
    Settings s;
    s.drive = 0.5f;
    s.type = 0;
    s.oversampling = 0;
    return s;
}

int latencyOf (int os)
{
    SatEngine e;
    e.prepare (sr, 512);
    return e.latencyFor (os);
}

void testCurves()
{
    const size_t from = 24000;
    auto in = sine (1000.0, 0.5, 48000);

    auto s = base();
    s.type = (int) woody::SatMode::Soft;
    auto soft = runMono (in, s);
    const double softH1 = bin (soft, 1000, from), softH2 = bin (soft, 2000, from), softH3 = bin (soft, 3000, from);
    check (toDb (softH3 / softH1) > -30.0 && toDb (softH3 / softH1) < -3.0, "Soft: third harmonic present", toDb (softH3 / softH1), -15.0);
    check (toDb (softH2 / softH1) < -60.0, "Soft: even harmonics absent (odd only)", toDb (softH2 / softH1), -60.0);

    s.type = (int) woody::SatMode::Tube;
    auto tube = runMono (in, s);
    const double tubeH2 = bin (tube, 2000, from) / bin (tube, 1000, from);
    check (toDb (tubeH2) > -30.0, "Tube: second harmonic present", toDb (tubeH2), -20.0);
    check (toDb (tubeH2) > toDb (softH2 / softH1) + 30.0, "Tube: far more even harmonics than Soft");
    double dc = 0.0;
    for (size_t i = from; i < tube.size(); ++i)
        dc += tube[i];
    check (std::abs (dc / (double) (tube.size() - from)) < 0.005, "Tube: DC blocked after the asymmetric curve", std::abs (dc / (double) (tube.size() - from)), 0.0);

    s.type = (int) woody::SatMode::Hard;
    s.drive = 1.0f;
    auto hard = runMono (in, s);
    double peak = 0.0;
    for (float v : hard)
        peak = std::max (peak, (double) std::abs (v));
    check (peak <= 1.001, "Hard: output never exceeds the clip level", peak, 1.0);
    check (rms (hard, from) > 0.95, "Hard: strongly driven sine becomes a square wave", rms (hard, from), 1.0);

    s.type = (int) woody::SatMode::Fold;
    s.drive = 0.6f;
    auto fold = runMono (in, s);
    const double foldDist = std::sqrt (std::max (0.0, rms (fold, from) * rms (fold, from) - 0.5 * std::pow (bin (fold, 1000, from), 2.0)));
    check (foldDist / rms (fold, from) > 0.3, "Fold: heavy harmonic content when folding", foldDist / rms (fold, from), 0.3);

    // A slow sine makes the anti-aliased output follow the transfer curve itself
    auto slow = sine (50.0, 0.3, 24000);
    s.drive = 0.5f;
    const double g = std::pow (10.0, 0.5 * maxDriveDb / 20.0);
    for (auto mode : { woody::SatMode::Soft, woody::SatMode::Tape, woody::SatMode::Hard, woody::SatMode::Fold })
    {
        s.type = (int) mode;
        auto out = runMono (slow, s);
        double error = 0.0;
        for (size_t i = 1000; i < out.size(); ++i)
            error += std::abs (out[i] - woody::sat::shape (mode, g * slow[i]));
        error /= (double) (out.size() - 1000);
        check (error < 0.01, "Curve shape matches the transfer function", error, 0.0);
    }
}

void testFinite()
{
    bool finite = true;
    double worst = 0.0;
    for (int type = 0; type < 5; ++type)
        for (int os = 0; os < 4; ++os)
            for (int stereo = 0; stereo < 3; ++stereo)
            {
                Settings s;
                s.type = type;
                s.oversampling = os;
                s.stereo = stereo;
                s.drive = 1.0f;
                s.bias = 1.0f;
                s.tone = 1.0f;
                s.autoGain = true;
                s.inputDb = 24.0f;
                s.outputDb = 12.0f;
                const bool focused = (type + os) % 2 == 1;
                const bool fullySaturated = ! focused && stereo == Stereo;
                if (focused)
                {
                    s.focusLow = 300.0f;
                    s.focusHigh = 8000.0f;
                }

                auto loud = noise (8000, 1.0e6f, (unsigned) (type * 7 + os));
                auto result = run (loud, loud, s);
                finite = finite && allFinite (result.left) && allFinite (result.right);
                if (fullySaturated)   // what is outside the focus band or the chosen channel passes untouched
                    worst = std::max (worst, rms (result.left));
            }

    check (finite, "Extreme input stays finite for every type, oversampling and stereo mode");
    check (worst < 1000.0, "Extreme input stays bounded", worst, 1000.0);
}

void testMixAndLatency()
{
    for (int os = 0; os < 4; ++os)
    {
        auto in = noise (20000, 0.5f);
        auto s = base();
        s.type = (int) woody::SatMode::Fold;
        s.mix = 0.0f;
        s.oversampling = os;

        const int latency = latencyOf (os);
        const auto out = runMono (in, s);
        const double error = maxDifference (out, in, latency);
        char label[100];
        std::snprintf (label, sizeof (label), "mix = 0 returns the dry signal exactly (oversampling index %d, latency %d)", os, latency);
        check (error < 1.0e-6, label, error, 0.0);
    }

    check (latencyOf (0) == 0, "No latency without oversampling");
    check (latencyOf (1) > 0 && latencyOf (2) >= latencyOf (1) && latencyOf (3) >= latencyOf (2), "Latency grows with the oversampling factor");

    // In the linear region the wet signal is the input: a half wet mix must stay aligned with the dry one
    for (int os = 1; os < 4; ++os)
    {
        auto in = sine (1000.0, 0.01, 24000);
        auto s = base();
        s.type = (int) woody::SatMode::Soft;
        s.drive = 0.0f;
        s.mix = 0.5f;
        s.oversampling = os;

        const int latency = latencyOf (os);
        const auto out = runMono (in, s);
        Signal expected (in.size());
        for (size_t i = 0; i < in.size(); ++i)
            expected[i] = i >= (size_t) latency ? in[i - (size_t) latency] : 0.0f;

        Signal diff (in.size());
        for (size_t i = 0; i < in.size(); ++i)
            diff[i] = out[i] - expected[i];
        const double relative = rms (diff, 12000) / rms (expected, 12000);
        char label[100];
        std::snprintf (label, sizeof (label), "Dry and wet stay phase-aligned with oversampling index %d", os);
        check (relative < 0.03, label, relative, 0.0);
    }

    auto in = noise (10000, 0.5f);
    auto s = base();
    s.bypass = true;
    s.oversampling = 2;
    check (maxDifference (runMono (in, s), in, latencyOf (2)) < 1.0e-6, "Bypass passes the delayed dry signal");
}

void testAutoGain()
{
    auto in = sine (1000.0, 0.3, 96000);
    const double reference = rms (in, 48000);

    for (int type : { 0, 1, 2 })
    {
        double worst = 0.0;
        for (float drive : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto s = base();
            s.type = type;
            s.drive = drive;
            s.autoGain = true;
            const auto out = runMono (in, s);
            worst = std::max (worst, std::abs (toDb (rms (out, 48000) / reference)));
        }
        char label[100];
        std::snprintf (label, sizeof (label), "Auto-gain keeps the level within 1.5 dB across drive (type %d)", type);
        check (worst < 1.5, label, worst, 1.5);
    }

    auto s = base();
    s.drive = 1.0f;
    check (toDb (rms (runMono (in, s), 48000) / reference) > 3.0, "Without auto-gain the level grows with drive");
}

void testStereoModes()
{
    auto side = sine (700.0, 0.4, 20000);
    Signal negative (side.size());
    for (size_t i = 0; i < side.size(); ++i)
        negative[i] = -side[i];

    auto s = base();
    s.drive = 1.0f;
    s.stereo = MidOnly;
    auto pureSide = run (side, negative, s);
    check (maxDifference (pureSide.left, side) < 1.0e-5 && maxDifference (pureSide.right, negative) < 1.0e-5,
           "Mid only leaves a pure side signal untouched");

    s.stereo = SideOnly;
    auto pureMid = run (side, side, s);
    check (maxDifference (pureMid.left, side) < 1.0e-5 && maxDifference (pureMid.right, side) < 1.0e-5,
           "Side only leaves a pure mid signal untouched");

    s.stereo = MidOnly;
    auto saturatedMid = run (side, side, s);
    check (maxDifference (saturatedMid.left, side) > 0.1, "Mid only does saturate a mid signal");

    s.stereo = Stereo;
    auto saturatedBoth = run (side, negative, s);
    check (maxDifference (saturatedBoth.left, side) > 0.1, "Stereo mode saturates both channels");
}

void testFocus()
{
    auto s = base();
    s.drive = 0.3f;
    s.focusLow = 1000.0f;
    s.focusHigh = 5000.0f;

    for (double freq : { 100.0, 12000.0 })
    {
        s.focusLow = freq < 1000.0 ? 1000.0f : 200.0f;
        s.focusHigh = freq < 1000.0 ? 5000.0f : 2000.0f;
        auto in = sine (freq, 0.5, 48000);
        auto out = runMono (in, s);
        Signal diff (in.size());
        for (size_t i = 0; i < in.size(); ++i)
            diff[i] = out[i] - in[i];
        const double relative = rms (diff, 24000) / rms (in, 24000);
        char label[100];
        std::snprintf (label, sizeof (label), "Focus leaves %.0f Hz (outside the band) unchanged", freq);
        check (relative < 0.03, label, relative, 0.0);
    }

    s.focusLow = 500.0f;
    s.focusHigh = 2000.0f;
    auto in = sine (1000.0, 0.5, 48000);
    auto out = runMono (in, s);
    check (toDb (bin (out, 3000, 24000) / bin (out, 1000, 24000)) > -30.0, "Focus still saturates inside the band");
}

void testOversampling()
{
    constexpr double freq = 7123.0;
    auto in = sine (freq, 0.8, 96000);
    auto s = base();
    s.type = (int) woody::SatMode::Fold;
    s.drive = 0.6f;

    auto aliasing = [&] (int os)
    {
        s.oversampling = os;
        const auto out = runMono (in, s);
        double harmonics = 0.0;
        for (int k = 1; k * freq < sr / 2.0; ++k)
            harmonics += std::pow (bin (out, k * freq, 48000), 2.0) * 0.5;
        const double r = rms (out, 48000);
        return std::sqrt (std::max (0.0, r * r - harmonics));
    };

    const double off = aliasing (0), x2 = aliasing (1), x4 = aliasing (2), x8 = aliasing (3);
    std::printf ("      aliasing level: off %.1f dB, 2x %.1f dB, 4x %.1f dB, 8x %.1f dB\n", toDb (off), toDb (x2), toDb (x4), toDb (x8));
    check (toDb (off) - toDb (x2) > 3.0, "2x oversampling reduces aliasing versus off", toDb (x2), toDb (off));
    check (toDb (off) - toDb (x4) > 6.0, "4x oversampling reduces aliasing clearly", toDb (x4), toDb (off));
    check (toDb (off) - toDb (x8) > 10.0, "8x oversampling reduces aliasing a lot", toDb (x8), toDb (off));
}

void testBlockSizes()
{
    auto in = noise (5000, 0.4f, 9);
    auto s = base();
    s.type = (int) woody::SatMode::Tape;
    s.oversampling = 2;
    s.focusLow = 150.0f;

    const auto big = run (in, in, s, 5000, 5000).left;
    const auto chunked = run (in, in, s, 5000, 64).left;
    const auto tiny = run (in, in, s, 7, 64).left;
    check (maxDifference (big, chunked) < 1.0e-4, "A block larger than the prepared size gives the same result", maxDifference (big, chunked), 0.0);
    check (maxDifference (big, tiny) < 1.0e-4, "Tiny blocks give the same result", maxDifference (big, tiny), 0.0);
}

void testLatencyAccuracy()
{
    for (int os = 1; os < 4; ++os)
    {
        const double freq = 500.0;
        auto in = sine (freq, 0.01, 24000);
        auto s = base();
        s.type = (int) woody::SatMode::Soft;
        s.drive = 0.0f;
        s.oversampling = os;
        const auto out = runMono (in, s);

        std::complex<double> a = 0.0, b = 0.0;
        for (size_t i = 12000; i < in.size(); ++i)
        {
            const auto w = std::polar (1.0, -2.0 * pi * freq * (double) i / sr);
            a += (double) in[i] * w;
            b += (double) out[i] * w;
        }
        const double delay = -std::arg (b / a) / (2.0 * pi * freq / sr);
        char label[100];
        std::snprintf (label, sizeof (label), "Reported latency is within 0.3 samples of the real delay (oversampling index %d)", os);
        const double period = sr / freq;
        const double error = std::fmod (delay - latencyOf (os) + 1.5 * period, period) - 0.5 * period;
        check (std::abs (error) < 0.3, label, error, 0.0);
    }
}
} // namespace

int main()
{
    testLatencyAccuracy();
    testCurves();
    testFinite();
    testMixAndLatency();
    testAutoGain();
    testStereoModes();
    testFocus();
    testOversampling();
    testBlockSizes();

    std::printf ("\n%d of %d checks passed, %d failed\n", total - failures, total, failures);
    return failures == 0 ? 0 : 1;
}
