#include "host.hpp"

#include <deque>
#include <iostream>
#include <stdexcept>

namespace {
std::deque<uint64_t> tb_values;
unsigned tb_reads = 0;
} // namespace

uint64_t mock_timebase_read() {
    if (tb_values.empty()) throw std::runtime_error("unexpected timebase read");
    const uint64_t value = tb_values.front();
    tb_values.pop_front();
    ++tb_reads;
    return value;
}

#define __rdtsc() mock_timebase_read()
PPC_EXTERN_FUNC(sub_82B8F9C0);

int main() {
    try {
        unsigned checks = 0;
        auto require = [&](bool condition) {
            if (!condition) throw std::runtime_error("timebase probe assertion failed");
            ++checks;
        };

        acv_probe::Memory memory;
        acv_probe::MemoryScope memory_scope(memory);
        uint8_t* base = memory.base();
        constexpr uint32_t page = 0x83885000;
        memory.commit(page, memory.page_size());
        PPCContext ctx{};

        // A zero low word takes the retail conditional reread path.
        tb_values = {0x1234567800000000ull, 0x0123456789abcdefull};
        tb_reads = 0;
        ctx.r3.u64 = page + 32;
        ctx.r4.u64 = 0xfeedfacecafebeefull;
        ctx.lr = 0x10203040;
        sub_82B8F9C0(ctx, base);
        require(tb_reads == 2 && tb_values.empty());
        require(PPC_LOAD_U64(page + 32) == 0x0123456789abcdefull);
        require(ctx.r3.u64 == 1 && ctx.r4.u64 == 0xfeedfacecafebeefull && ctx.lr == 0x10203040);

        // A nonzero low word keeps the first whole-register sample.
        tb_values = {0xfedcba9876543210ull};
        tb_reads = 0;
        ctx.r3.u64 = page + 40;
        sub_82B8F9C0(ctx, base);
        require(tb_reads == 1 && tb_values.empty());
        require(PPC_LOAD_U64(page + 40) == 0xfedcba9876543210ull);
        require(ctx.r3.u64 == 1);

        std::cout << "{\"checks\":" << checks
                  << ",\"generated_function\":\"0x82B8F9C0\",\"status\":\"passed\"}\n";
    } catch (const std::exception& e) {
        std::cerr << "timebase probe failed: " << e.what() << '\n';
        return 1;
    }
}
