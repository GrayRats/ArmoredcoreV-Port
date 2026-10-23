// Standalone validation of the isolated XenonRecomp vnor128 host expression.
#include <array>
#include <cstdint>
#include <cstdio>
#include <emmintrin.h>

int main() {
    alignas(16) std::array<std::uint8_t, 16> a{};
    alignas(16) std::array<std::uint8_t, 16> b{};
    alignas(16) std::array<std::uint8_t, 16> out{};
    for (unsigned x = 0; x < 256; ++x) {
        for (unsigned y = 0; y < 256; ++y) {
            for (std::size_t i = 0; i < 16; ++i) {
                a[i] = static_cast<std::uint8_t>(x + i * 17);
                b[i] = static_cast<std::uint8_t>(y + i * 29);
            }
            const __m128i lhs = _mm_load_si128(reinterpret_cast<const __m128i*>(a.data()));
            const __m128i rhs = _mm_load_si128(reinterpret_cast<const __m128i*>(b.data()));
            const __m128i actual = _mm_xor_si128(_mm_or_si128(lhs, rhs), _mm_set1_epi32(-1));
            _mm_store_si128(reinterpret_cast<__m128i*>(out.data()), actual);
            for (std::size_t i = 0; i < 16; ++i) {
                const auto expected = static_cast<std::uint8_t>(~(a[i] | b[i]));
                if (out[i] != expected) {
                    std::fprintf(stderr, "FAIL x=%u y=%u byte=%zu\n", x, y, i);
                    return 1;
                }
            }
        }
    }
    std::puts("PASS: vnor128 host expression matched bytewise ~ (A | B) in 1,048,576 lane checks");
    return 0;
}
