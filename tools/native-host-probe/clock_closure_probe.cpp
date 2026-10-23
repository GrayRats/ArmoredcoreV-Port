#include "host.hpp"
#include <array>
#include <iostream>

PPC_EXTERN_FUNC(sub_822C8AD8);
namespace {
std::array<uint64_t, 2> samples{};
unsigned reads = 0;
}
uint64_t mock_timebase_read() {
    if (reads == samples.size()) throw std::runtime_error("unexpected clock read");
    return samples[reads++];
}

int main() {
    try {
        unsigned checks = 0, cases = 0;
        auto require = [&](bool value) {
            if (!value) throw std::runtime_error("clock closure assertion failed");
            ++checks;
        };
        acv_probe::Memory memory;
        acv_probe::MemoryScope memory_scope(memory);
        uint8_t* base = memory.base();
        constexpr uint32_t stack = 0x70001000;
        memory.commit(stack - memory.page_size(), memory.page_size() * 2);
        // @retail 0x822C8AD8: 96-byte frame, result at old sp-16,
        // saved 32-bit LR at old sp-8. Synthetic stack, no Xbox thread setup.
        uint64_t random = 0x123456789abcdef0ull;
        for (unsigned i = 0; i < 2048; ++i) {
            random ^= random << 13; random ^= random >> 7; random ^= random << 17;
            for (bool reread : {false, true}) {
                samples = {reread ? (random & 0xffffffff00000000ull) : (random | 1),
                           ~random};
                reads = 0;
                PPCContext ctx{};
                ctx.r1.u64 = stack;
                ctx.r4.u64 = 0x0123456789abcdefull;
                ctx.r31.u64 = 0xfedcba9876543210ull;
                // Include high LR bits: stw/lwz explicitly discard them.
                ctx.lr = 0xaabbccdd12345678ull;
                PPC_STORE_U32(stack - 100, 0x89abcdefu);
                PPC_STORE_U32(stack - 4, 0x76543210u);
                sub_822C8AD8(ctx, base);
                const uint64_t expected = samples[reread ? 1 : 0];
                require(ctx.r3.u64 == expected);
                require(reads == (reread ? 2u : 1u));
                require(PPC_LOAD_U64(stack - 16) == expected);
                require(ctx.r1.u64 == stack && PPC_LOAD_U32(stack - 96) == stack);
                require(ctx.lr == 0x12345678 && PPC_LOAD_U32(stack - 8) == 0x12345678);
                require(ctx.r4.u64 == 0x0123456789abcdefull && ctx.r31.u64 == 0xfedcba9876543210ull);
                require(PPC_LOAD_U32(stack - 100) == 0x89abcdefu &&
                        PPC_LOAD_U32(stack - 4) == 0x76543210u);
                require(ctx.r11.u64 == expected && ctx.r10.u64 == uint32_t(samples[0]));
                ++cases;
            }
        }
        std::cout << "{\"checks\":" << checks << ",\"cases\":" << cases
                  << ",\"generated_functions\":2,\"status\":\"passed\"}\n";
    } catch (const std::exception& e) {
        std::cerr << "clock closure probe failed: " << e.what() << '\n';
        return 1;
    }
}
