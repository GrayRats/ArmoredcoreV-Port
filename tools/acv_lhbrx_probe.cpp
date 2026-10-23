// Host-endian check for the isolated XenonRecomp lhbrx emission formula.
#include <cstdint>
#include <cstdio>

static std::uint16_t PPC_LOAD_U16(const volatile std::uint16_t* p) {
    const auto host = *p;
    return __builtin_bswap16(host);
}

int main() {
    // Host little-endian words representing guest byte pairs 12 34, AB CD, EF F0.
    alignas(2) volatile std::uint16_t memory[3] = {0x3412, 0xCDAB, 0xF0EF};
    for (unsigned pair = 0; pair < 3; ++pair) {
        const volatile std::uint16_t* p = memory + pair;
        const std::uint16_t ordinary_lhz = PPC_LOAD_U16(p);
        // Mirrors the proposed emitter: bswap16(PPC_LOAD_U16(effective_address)).
        const std::uint16_t byte_reversed = __builtin_bswap16(PPC_LOAD_U16(p));
        const std::uint16_t expected_lhz[] = {0x1234, 0xABCD, 0xEFF0};
        const std::uint16_t expected_lhbrx[] = {0x3412, 0xCDAB, 0xF0EF};
        if (ordinary_lhz != expected_lhz[pair] || byte_reversed != expected_lhbrx[pair]) {
            std::fprintf(stderr, "FAIL pair=%u lhz=%04x lhbrx=%04x\n",
                         pair, ordinary_lhz, byte_reversed);
            return 1;
        }
    }
    std::puts("PASS: big-endian PPC_LOAD_U16 plus bswap matches lhbrx for three aligned halfwords");
    return 0;
}
