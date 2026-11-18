#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <vector>

namespace duster::test {

inline std::vector<float> sine(double fs, double hz, double seconds, float amp) {
    std::vector<float> out(static_cast<std::size_t>(fs * seconds));
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] =
            amp * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / fs));
    return out;
}

inline double toDb(double ratio) {
    return 20.0 * std::log10(ratio);
}

inline float rms(const std::vector<float>& v, std::size_t from, std::size_t to) {
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(v[i]) * v[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(std::max<std::size_t>(1, to - from))));
}

// Steady-state gain in dB of a per-sample process(x) at one frequency.
template <typename Process> float measureGainDb(Process&& process, float fs, float hz, float amp = 0.1f) {
    const auto in = sine(fs, hz, 1.0, amp);
    std::vector<float> out(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
        out[i] = process(in[i]);
    const std::size_t half = in.size() / 2;
    return static_cast<float>(toDb(rms(out, half, in.size()) / rms(in, half, in.size())));
}

// Magnitudes of a real signal's spectrum, bins 0 to N/2.
struct Spectrum {
    std::vector<double> magnitude;
    double binHz = 0.0;

    double peakNear(double hz, double widthHz) const {
        double best = 0.0;
        for (std::size_t k = binOf(hz - widthHz); k <= binOf(hz + widthHz); ++k)
            best = std::max(best, magnitude[k]);
        return best;
    }

    double peakHz(double loHz, double hiHz) const {
        std::size_t best = binOf(loHz);
        for (std::size_t k = best; k <= binOf(hiHz); ++k)
            if (magnitude[k] > magnitude[best])
                best = k;
        return static_cast<double>(best) * binHz;
    }

    std::size_t binOf(double hz) const {
        const double bin = std::clamp(std::round(hz / binHz), 0.0, static_cast<double>(magnitude.size() - 1));
        return static_cast<std::size_t>(bin);
    }
};

inline void fftInPlace(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const auto step = std::polar(1.0, -2.0 * std::numbers::pi / static_cast<double>(len));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w = 1.0;
            for (std::size_t k = 0; k < len / 2; ++k, w *= step) {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

// Spectrum of x[from, from + count). The Blackman-Harris window (sidelobes near -92 dB) is applied to
// the segment before zero-padding to a power of two, so padding never leaks into the window.
inline Spectrum spectrum(const std::vector<float>& x, double fs, std::size_t from = 0,
                         std::size_t count = std::numeric_limits<std::size_t>::max()) {
    count = std::min(count, x.size() - from);
    std::size_t n = 1;
    while (n < count)
        n <<= 1;
    std::vector<std::complex<double>> a(n);
    const double span = static_cast<double>(count - 1);
    for (std::size_t i = 0; i < count; ++i) {
        const double p = 2.0 * std::numbers::pi * static_cast<double>(i) / span;
        const double w =
            0.35875 - 0.48829 * std::cos(p) + 0.14128 * std::cos(2 * p) - 0.01168 * std::cos(3 * p);
        a[i] = w * x[from + i];
    }
    fftInPlace(a);
    Spectrum s;
    s.binHz = fs / static_cast<double>(n);
    s.magnitude.resize(n / 2 + 1);
    for (std::size_t k = 0; k < s.magnitude.size(); ++k)
        s.magnitude[k] = std::abs(a[k]);
    return s;
}

} // namespace duster::test
