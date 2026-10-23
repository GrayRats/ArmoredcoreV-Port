#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace acv_decomp {
enum class ScalarArithmetic { GeneratedSplit, FusedCandidate };

// Values must come from the observed guest image, not host libm constants.
struct Constants830d68f0 {
    double inverse_pi, pi_high, pi_low, limit;
    std::array<double, 9> coefficients; // constant term through degree 8
};

// @retail 0x830D68F0
// Status: partial, finite-domain reconstruction; return value only.
// GeneratedSplit matches selected generated C++ with contraction disabled.
// FusedCandidate is an explicit rounding experiment, NOT verified guest parity.
// Requires FE_TONEAREST, FTZ/DAZ off, finite input with abs(input)<table limit.
inline double scalar_830d68f0(double input, const Constants830d68f0& c,
                             ScalarArithmetic arithmetic) {
    const double magnitude = std::abs(input);
    if (!std::isfinite(input) || !(magnitude < c.limit))
        throw std::domain_error("0x830D68F0: outside reconstructed finite domain");
    if (magnitude == 0.0) return input; // retail leaves f1 unchanged for both zeros
    const double scaled = magnitude * c.inverse_pi;
    const int64_t interval = static_cast<int64_t>(std::nearbyint(scaled));
    const double index = static_cast<double>(interval);
    auto negative_multiply_subtract = [&](double a, double b, double d) {
        if (arithmetic == ScalarArithmetic::FusedCandidate) return -std::fma(a, b, -d);
        return -(a * b - d);
    };
    const double high_remainder = negative_multiply_subtract(c.pi_high, index, magnitude);
    const double remainder = negative_multiply_subtract(c.pi_low, index, high_remainder);
    const double square = remainder * remainder;
    double polynomial = c.coefficients[8];
    for (int i = 7; i >= 0; --i) {
        if (arithmetic == ScalarArithmetic::FusedCandidate)
            polynomial = std::fma(polynomial, square, c.coefficients[i]);
        else
            polynomial = polynomial * square + c.coefficients[i];
    }
    double result = polynomial * remainder;
    if (interval & 1) // fneg toggles the sign bit
        result = std::bit_cast<double>(std::bit_cast<uint64_t>(result) ^ (UINT64_C(1) << 63));
    return result * (input >= 0.0 ? 1.0 : -1.0);
}
// @retail 0x830D69D0
// Status/confidence exclusions as above; extra half_pi from guest table +0.
// Reconstructed domain additionally requires abs(input)+half_pi<table limit.
inline double scalar_830d69d0(double input, const Constants830d68f0& c,
                             double half_pi, ScalarArithmetic arithmetic) {
    const double magnitude = std::abs(input);
    const double shifted = half_pi + magnitude;
    if (!std::isfinite(input) || !(shifted < c.limit))
        throw std::domain_error("0x830D69D0: outside reconstructed finite domain");
    if (magnitude == 0.0) return 1.0;
    const int64_t interval = static_cast<int64_t>(std::nearbyint(c.inverse_pi * shifted));
    const double index = static_cast<double>(interval) - 0.5; // lfs table+36
    auto negative_multiply_subtract = [&](double a, double b, double d) {
        if (arithmetic == ScalarArithmetic::FusedCandidate) return -std::fma(a, b, -d);
        return -(a * b - d);
    };
    const double high_remainder = negative_multiply_subtract(c.pi_high, index, magnitude);
    const double remainder = negative_multiply_subtract(c.pi_low, index, high_remainder);
    const double square = remainder * remainder;
    double polynomial = c.coefficients[8];
    for (int i = 7; i >= 0; --i) {
        if (arithmetic == ScalarArithmetic::FusedCandidate)
            polynomial = std::fma(polynomial, square, c.coefficients[i]);
        else
            polynomial = polynomial * square + c.coefficients[i];
    }
    double result = polynomial * remainder;
    if (interval & 1)
        result = std::bit_cast<double>(std::bit_cast<uint64_t>(result) ^ (UINT64_C(1) << 63));
    return result;
}
} // namespace acv_decomp
