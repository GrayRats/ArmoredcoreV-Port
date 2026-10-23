#include "frequency_service.hpp"
#include <iostream>

PPC_EXTERN_FUNC(sub_82B8FA70);
int main() {
    try {
        unsigned checks = 0, cases = 0;
        auto require = [&](bool value) {
            if (!value) throw std::runtime_error("frequency import assertion failed");
            ++checks;
        };
        acv_probe::Memory memory;
        acv_probe::MemoryScope memory_scope(memory);
        uint8_t* base = memory.base();
        constexpr uint32_t stack = 0x70001000, output = 0x83885020;
        memory.commit(stack - memory.page_size(), memory.page_size());
        memory.commit(output & ~(memory.page_size() - 1), memory.page_size());
        // 50,000,000 is one synthetic fixture, not a measured Xenon frequency.
        for (uint32_t frequency : {0u, 1u, 50000000u, 0x80000000u, 0xffffffffu}) {
            acv_probe::FrequencyScope frequency_scope(frequency);
            PPCContext ctx{};
            ctx.r1.u64 = stack;
            ctx.r3.u64 = output;
            ctx.r31.u64 = 0xfedcba9876543210ull;
            ctx.r4.u64 = 0xabcdef0123456789ull;
            ctx.lr = 0x12345678;
            PPC_STORE_U32(output - 4, 0x89abcdefu);
            PPC_STORE_U32(output + 8, 0x76543210u);
            sub_82B8FA70(ctx, base);
            require(ctx.r3.u64 == 1);
            require(PPC_LOAD_U64(output) == frequency);
            auto bytes = memory.checked(base, output, 8);
            require(bytes[0] == 0 && bytes[3] == 0 && bytes[4] == uint8_t(frequency >> 24)
                    && bytes[7] == uint8_t(frequency));
            require(ctx.r1.u64 == stack && PPC_LOAD_U32(stack - 96) == stack);
            require(ctx.lr == 0x12345678 && ctx.r31.u64 == 0xfedcba9876543210ull);
            require(PPC_LOAD_U64(stack - 16) == 0xfedcba9876543210ull &&
                    PPC_LOAD_U32(stack - 8) == 0x12345678);
            require(ctx.r4.u64 == 0xabcdef0123456789ull);
            require(PPC_LOAD_U32(output - 4) == 0x89abcdefu && PPC_LOAD_U32(output + 8) == 0x76543210u);
            ++cases;
        }
        PPCContext ctx{};
        auto require_no_policy = [&] {
            bool rejected = false;
            try { __imp__KeQueryPerformanceFrequency(ctx, base); }
            catch (const std::logic_error&) { rejected = true; }
            require(rejected);
        };
        require_no_policy();
        {
            acv_probe::FrequencyScope outer(19);
            {
                acv_probe::FrequencyScope inner(23);
                __imp__KeQueryPerformanceFrequency(ctx, base);
                require(ctx.r3.u64 == 23);
            }
            __imp__KeQueryPerformanceFrequency(ctx, base);
            require(ctx.r3.u64 == 19);
        }
        require_no_policy();
        std::cout << "{\"checks\":" << checks << ",\"cases\":" << cases
                  << ",\"generated_functions\":1,\"imported_services\":1,\"status\":\"passed\"}\n";
    } catch (const std::exception& e) {
        std::cerr << "frequency import probe failed: " << e.what() << '\n';
        return 1;
    }
}
