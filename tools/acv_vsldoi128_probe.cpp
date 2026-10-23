// Read-only host probe for the emitted BBB_MoreInstructions vsldoi128 expression.
// @retail 0x82F12B9C — classification only; this probe tests the handler formula.
#include <array>
#include <cstdint>
#include <cstdio>
#include <emmintrin.h>
#include <tmmintrin.h>

template <int Shift>
bool test_shift(const std::array<std::uint8_t, 16>& guest_a,
                const std::array<std::uint8_t, 16>& guest_b,
                const std::array<std::uint8_t, 16>& host_a,
                const std::array<std::uint8_t, 16>& host_b) {
    alignas(16) std::array<std::uint8_t, 16> host_out{};
    alignas(16) std::array<std::uint8_t, 16> guest_out{};
    const __m128i result = _mm_alignr_epi8(
        _mm_load_si128(reinterpret_cast<const __m128i*>(host_a.data())),
        _mm_load_si128(reinterpret_cast<const __m128i*>(host_b.data())),
        16 - Shift);
    _mm_store_si128(reinterpret_cast<__m128i*>(host_out.data()), result);
    for (std::size_t i = 0; i < 16; ++i)
        guest_out[i] = host_out[15 - i];

    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t source_index = Shift + i;
        const std::uint8_t expected = source_index < 16
            ? guest_a[source_index]
            : guest_b[source_index - 16];
        if (guest_out[i] != expected) {
            std::fprintf(stderr,
                "FAIL shift=%d byte=%zu got=%02x expected=%02x\n",
                Shift, i, guest_out[i], expected);
            return false;
        }
    }
    return true;
}

int main() {
    alignas(16) std::array<std::uint8_t, 16> guest_a{};
    alignas(16) std::array<std::uint8_t, 16> guest_b{};
    alignas(16) std::array<std::uint8_t, 16> host_a{};
    alignas(16) std::array<std::uint8_t, 16> host_b{};
    for (std::size_t i = 0; i < 16; ++i) {
        guest_a[i] = static_cast<std::uint8_t>(0x10 + i);
        guest_b[i] = static_cast<std::uint8_t>(0x80 + i);
        // XenonRecomp stores PPC vector bytes in reversed host-array order.
        host_a[15 - i] = guest_a[i];
        host_b[15 - i] = guest_b[i];
    }

    // Each intrinsic immediate must be a compile-time constant.
    if (!test_shift<0>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<1>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<2>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<3>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<4>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<5>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<6>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<7>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<8>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<9>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<10>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<11>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<12>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<13>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<14>(guest_a, guest_b, host_a, host_b) ||
        !test_shift<15>(guest_a, guest_b, host_a, host_b))
        return 1;

    std::puts("PASS: vsldoi128 emitted alignr expression matches PPC byte semantics for shifts 0..15");
    return 0;
}
