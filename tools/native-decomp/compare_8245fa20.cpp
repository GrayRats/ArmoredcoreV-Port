#include "acv_8245fa20.hpp"
#include "host.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
#include <xmmintrin.h>

PPC_EXTERN_FUNC(sub_8245FA20);
using acv_decomp::TimerState;

int main() {
    try {
        if (std::fesetround(FE_TONEAREST) || std::fegetround() != FE_TONEAREST)
            throw std::runtime_error("round-to-nearest setup failed");
        const unsigned mxcsr = (_mm_getcsr() & ~(0x8000u | 0x0040u)); // FTZ/DAZ off
        _mm_setcsr(mxcsr);
        acv_probe::Memory memory;
        acv_probe::MemoryScope scope(memory);
        uint8_t* base = memory.base();
        constexpr uint32_t object = 0x83885080, stack = 0x70000800;
        memory.commit(0x83885000, memory.page_size());
        memory.commit(0x70000000, memory.page_size());
        uint64_t cases = 0, float_comparisons = 0, nan_classifications = 0, nan_payload_differences = 0;
        auto equal_float = [&](float expected, float actual) {
            if (std::isnan(expected) || std::isnan(actual)) {
                ++nan_classifications;
                if (std::bit_cast<uint32_t>(expected) != std::bit_cast<uint32_t>(actual)) ++nan_payload_differences;
                return std::isnan(expected) && std::isnan(actual);
            }
            ++float_comparisons;
            return std::bit_cast<uint32_t>(expected) == std::bit_cast<uint32_t>(actual);
        };
        auto compare = [&](TimerState input, double dt) {
            _mm_setcsr(mxcsr);
            PPC_STORE_U64(object - 8, uint64_t{0x123456789abcdef0});
            PPC_STORE_U64(object + 12, uint64_t{0xfedcba9876543210});
            PPC_STORE_U8(object, input.active);
            PPC_STORE_U8(object + 1, input.mode);
            PPC_STORE_U16(object + 2, input.counter);
            PPC_STORE_U32(object + 4, std::bit_cast<uint32_t>(input.period));
            PPC_STORE_U32(object + 8, std::bit_cast<uint32_t>(input.accumulator));
            PPCContext ctx{};
            ctx.r1.u64 = stack;
            ctx.r3.u64 = object;
            ctx.f1.f64 = dt;
            sub_8245FA20(ctx, base);
            TimerState actual{PPC_LOAD_U8(object), PPC_LOAD_U8(object + 1), PPC_LOAD_U16(object + 2),
                std::bit_cast<float>(PPC_LOAD_U32(object + 4)), std::bit_cast<float>(PPC_LOAD_U32(object + 8))};
            _mm_setcsr(mxcsr);
            TimerState expected = input;
            acv_decomp::advance_timer(expected, dt);
            ++cases;
            if (expected.active != actual.active || expected.mode != actual.mode || expected.counter != actual.counter ||
                !equal_float(expected.period, actual.period) || !equal_float(expected.accumulator, actual.accumulator) ||
                PPC_LOAD_U64(object - 8) != 0x123456789abcdef0 ||
                PPC_LOAD_U64(object + 12) != 0xfedcba9876543210) {
                std::cerr << "mismatch case " << cases << " mode " << unsigned(input.mode) << '\n';
                throw std::runtime_error("state comparison failed");
            }
        };
        struct Scenario { float accumulator; float period; double dt; };
        const float inf = std::numeric_limits<float>::infinity();
        const float nan = std::bit_cast<float>(uint32_t{0x7fc12345});
        const std::vector<Scenario> scenarios = {
            {0, 0.25f, 0}, {0, 0.25f, 0.25}, {0, 0.25f, 2.0},
            {1, 0.25f, -1.5}, {-0.0f, 0.0f, -0.0}, {0, -1, 0},
            {inf, 1, 0}, {-inf, 1, 0}, {1, inf, inf}, {inf, inf, 0},
            {nan, 1, 0}, {0, nan, 1}, {1, 2, double(nan)},
            {1, 2, 0x1.0000001p-24}, // double input crosses float addition midpoint
            {1, 2, 0x1.ffffffep-25},
            {std::numeric_limits<float>::denorm_min(), 1, 0},
        };
        for (unsigned mode = 0; mode < 256; ++mode)
            for (uint8_t active : {uint8_t{0}, uint8_t{1}, uint8_t{255}})
                for (uint16_t count : {uint16_t{0}, uint16_t{1}, uint16_t{65534}, uint16_t{65535}})
                    for (auto s : scenarios) compare({active, uint8_t(mode), count, s.period, s.accumulator}, s.dt);
        std::mt19937 random(0x8245fa20);
        for (unsigned i = 0; i < 20000; ++i) {
            // All float bit patterns, including nonfinite/denormal state, are intentional.
            TimerState state{uint8_t(random()), uint8_t(i % 8), uint16_t(random()),
                std::bit_cast<float>(uint32_t(random())), std::bit_cast<float>(uint32_t(random()))};
            double dt = double(int32_t(random())) / 100000.0;
            compare(state, dt);
        }
        // Independent expected-state checks prevent a mirrored-control-flow test alone.
        unsigned golden = 0;
        auto check = [&](TimerState state, double dt, uint8_t active, uint16_t counter, float accumulator) {
            acv_decomp::advance_timer(state, dt);
            if (state.active != active || state.counter != counter || !equal_float(state.accumulator, accumulator))
                throw std::runtime_error("expected transition failed");
            ++golden;
        };
        check({0, 1, 9, 0.25f, 1}, 10, 0, 9, 1);
        check({1, 1, 65535, 0.25f, 0}, 0.25, 1, 65535, 0);
        check({1, 3, 65534, 0.25f, 0}, 2, 1, 65535, 1.75f);
        check({1, 2, 9, 0.25f, 0}, 0.25, 0, 9, 0.25f);
        check({1, 4, 9, 0.25f, 1}, -2, 1, 9, -1);
        check({1, 0, 9, 0.25f, 1}, 0.5, 1, 9, 1.5f);
        check({1, 255, 9, 0.25f, 1}, 0.5, 1, 9, 1.5f);
        check({1, 1, 9, nan, 0}, 0, 1, 10, nan);
        check({1, 2, 9, nan, 0}, 0, 0, 9, 0);
        check({1, 1, 9, 2, 1}, 0x1.0000001p-24, 1, 9, std::nextafter(1.0f, 2.0f));
        std::cout << "{\"state_cases\":" << cases << ",\"golden_transitions\":" << golden
                  << ",\"bit_exact_non_nan_float_comparisons\":" << float_comparisons
                  << ",\"nan_classification_comparisons\":" << nan_classifications
                  << ",\"nan_payload_differences\":" << nan_payload_differences
                  << ",\"rounding\":\"FE_TONEAREST; FTZ/DAZ off\",\"status\":\"passed\"}\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
