#pragma once

#include <algorithm>
#include <cmath>

namespace woody
{
enum class SatMode { Soft, Tube, Tape, Hard, Fold };

constexpr const char* satModeChoices = "Soft|Tube|Tape|Hard|Fold";

namespace sat
{
constexpr double tubeBias = 0.35;

inline double logCosh (double x)
{
    const double a = std::abs (x);
    return a > 15.0 ? a - 0.6931471805599453 : std::log (std::cosh (x));
}

// The transfer curve
inline double shape (SatMode mode, double x)
{
    switch (mode)
    {
        case SatMode::Soft: return std::tanh (x);
        case SatMode::Tube: return std::tanh (x + tubeBias) - std::tanh (tubeBias);   // asymmetric: adds even harmonics
        case SatMode::Tape: return x / (1.0 + std::abs (x));
        case SatMode::Hard: return std::clamp (x, -1.0, 1.0);
        case SatMode::Fold: return std::sin (x);
    }
    return x;
}

// Its antiderivative, needed by the anti-aliasing
inline double antiderivative (SatMode mode, double x)
{
    switch (mode)
    {
        case SatMode::Soft: return logCosh (x);
        case SatMode::Tube: return logCosh (x + tubeBias) - x * std::tanh (tubeBias);
        case SatMode::Tape: return std::abs (x) - std::log (1.0 + std::abs (x));
        case SatMode::Hard: return std::abs (x) <= 1.0 ? 0.5 * x * x : std::abs (x) - 0.5;
        case SatMode::Fold: return -std::cos (x);
    }
    return 0.5 * x * x;
}
} // namespace sat

// First-order antiderivative anti-aliasing (ADAA): the output is the average of the curve
// between this sample and the previous one, which suppresses the aliasing of the new
// harmonics without oversampling and without adding latency.
class Saturator
{
public:
    void reset() { previous = 0.0; }

    float process (SatMode mode, float x)
    {
        const double current = x;
        const double delta = current - previous;

        const double y = std::abs (delta) < 1.0e-6
                             ? sat::shape (mode, 0.5 * (current + previous))
                             : (sat::antiderivative (mode, current) - sat::antiderivative (mode, previous)) / delta;

        previous = current;
        return (float) y;
    }

private:
    double previous = 0.0;
};

// One-pole high-pass that removes the DC an asymmetric curve creates.
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f;

    float process (float x, float r = 0.995f)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    void reset() { x1 = y1 = 0.0f; }
};
} // namespace woody
