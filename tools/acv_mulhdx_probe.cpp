// Standalone validation vectors for the isolated XenonRecomp mulhdx expression.
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>

struct TestCase { std::uint64_t a, b, expected; };

int main() {
    constexpr std::array<TestCase, 9> cases{{
        {0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL},
        {0x0000000000000001ULL, 0x0000000000000001ULL, 0x0000000000000000ULL},
        {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL},
        {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL, 0x3FFFFFFFFFFFFFFFULL},
        {0x8000000000000000ULL, 0x8000000000000000ULL, 0x4000000000000000ULL},
        {0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL},
        {0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL},
        {0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL, 0x0121FA00AD77D742ULL},
        {0xDEADBEEF01234567ULL, 0x13579BDF2468ACE0ULL, 0xFD7B7DEDC785D7F4ULL},
    }};
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto a = std::bit_cast<std::int64_t>(cases[i].a);
        const auto b = std::bit_cast<std::int64_t>(cases[i].b);
        const auto actual = static_cast<std::uint64_t>((__int128_t(a) * __int128_t(b)) >> 64);
        if (actual != cases[i].expected) {
            std::fprintf(stderr, "FAIL case=%zu got=%016llx expected=%016llx\n", i,
                static_cast<unsigned long long>(actual),
                static_cast<unsigned long long>(cases[i].expected));
            return 1;
        }
    }
    std::puts("PASS: mulhdx signed high-half matched 9 edge and mixed-sign vectors");
    return 0;
}
