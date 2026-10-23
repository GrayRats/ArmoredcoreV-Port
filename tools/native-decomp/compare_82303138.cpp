#include "acv_82303138.hpp"
#include "host.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <random>

PPC_EXTERN_FUNC(sub_82303138);
namespace {
using acv_decomp::Inputs82303138;
using acv_decomp::Call82302520;
constexpr uint32_t object = 0x40586000, stack = 0x70000800;
struct Memory {
    uint8_t* base;
    uint32_t load32(uint32_t address) { return PPC_LOAD_U32(address); }
    void store32(uint32_t address, uint32_t value) { PPC_STORE_U32(address, value); }
};
Inputs82303138 capture(const PPCContext& ctx) {
    return {{ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r6.u64,
             ctx.r7.u64, ctx.r8.u64, ctx.r9.u64, ctx.r10.u64},
            {std::bit_cast<uint64_t>(ctx.f1.f64), std::bit_cast<uint64_t>(ctx.f2.f64),
             std::bit_cast<uint64_t>(ctx.f3.f64), std::bit_cast<uint64_t>(ctx.f4.f64)}};
}
void apply(PPCContext& ctx, const Inputs82303138& in) {
    ctx.r3.u64 = in.r3_to_r10[0]; ctx.r4.u64 = in.r3_to_r10[1];
    ctx.r5.u64 = in.r3_to_r10[2]; ctx.r6.u64 = in.r3_to_r10[3];
    ctx.r7.u64 = in.r3_to_r10[4]; ctx.r8.u64 = in.r3_to_r10[5];
    ctx.r9.u64 = in.r3_to_r10[6]; ctx.r10.u64 = in.r3_to_r10[7];
    ctx.f1.f64 = std::bit_cast<double>(in.f1_to_f4[0]);
    ctx.f2.f64 = std::bit_cast<double>(in.f1_to_f4[1]);
    ctx.f3.f64 = std::bit_cast<double>(in.f1_to_f4[2]);
    ctx.f4.f64 = std::bit_cast<double>(in.f1_to_f4[3]);
}
unsigned calls;
Call82302520 observed{};
// Instrumented dependency boundary, deliberately not the original implementation.
void dependency_effect(Memory& mem, const Call82302520& call) {
    ++calls;
    observed = call;
    const auto pointer = static_cast<uint32_t>(call.inputs.r3_to_r10[0]);
    mem.store32(pointer + 0x20, 0xDECAFBAD);
}
void require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}
} // namespace
PPC_FUNC(sub_82302520) {
    Memory memory{base};
    dependency_effect(memory, {capture(ctx), PPC_LOAD_U32(ctx.r1.u32 + 84)});
    // Volatile return value may change: wrapper must not use it as object pointer.
    ctx.r3.u64 = 0x12345678;
}

int main() {
    try {
        acv_probe::Memory guest;
        acv_probe::MemoryScope scope(guest);
        uint8_t* base = guest.base();
        guest.commit(object, guest.page_size());
        guest.commit(0x70000000, guest.page_size());
        guest.commit(0x82001000, guest.page_size());
        PPC_STORE_U32(0x820018A0, 0x3F800000);
        Memory memory{base};
        std::mt19937_64 rng(0x82303138);
        unsigned count = 0, delegated = 0, writes_only = 0;
        auto check = [&](uint32_t flag, Inputs82303138 input) {
            std::array<uint8_t, 512> initial{}, translated{}, readable{}, expected{};
            for (auto& byte : initial) byte = uint8_t(rng());
            input.r3_to_r10[0] = object;
            std::memcpy(base + object, initial.data(), initial.size());
            PPC_STORE_U32(object + 0x1D0, flag);
            std::memcpy(initial.data(), base + object, initial.size());
            PPCContext ctx{};
            apply(ctx, input);
            ctx.r1.u64 = stack;
            ctx.lr = 0x82345678;
            calls = 0;
            sub_82303138(ctx, base);
            const unsigned actual_calls = calls;
            const auto actual_call = observed;
            require(ctx.r1.u32 == stack && ctx.lr == 0x82345678, "generated epilogue");
            std::memcpy(translated.data(), base + object, translated.size());
            std::memcpy(base + object, initial.data(), initial.size());
            calls = 0;
            acv_decomp::update_82303138(memory, input, dependency_effect);
            std::memcpy(readable.data(), base + object, readable.size());
            require(translated == readable, "object bytes differ");
            require(actual_calls == calls && calls == (flag == 0 ? 1u : 0u), "call count");
            // Independent expected byte effects, including guard/untouched bytes.
            expected = initial;
            auto expected_word = [&](unsigned offset, uint32_t value) {
                for (unsigned i = 0; i < 4; ++i) expected[offset + i] = uint8_t(value >> ((3-i)*8));
            };
            if (flag != 0) {
                ++writes_only;
                expected_word(0x4C, uint32_t(input.r3_to_r10[5]));
                expected_word(0x50, uint32_t(input.r3_to_r10[6]));
            } else {
                ++delegated;
                expected_word(0x20, 0xDECAFBAD);
                require(actual_call.inputs.r3_to_r10 == observed.inputs.r3_to_r10 &&
                        actual_call.inputs.f1_to_f4 == observed.inputs.f1_to_f4 &&
                        actual_call.outgoing_stack_84 == observed.outgoing_stack_84, "callee inputs differ");
                auto wanted = input;
                wanted.r3_to_r10[7] = 0;
                wanted.f1_to_f4[3] = 0x3FF0000000000000;
                require(observed.inputs.r3_to_r10 == wanted.r3_to_r10 &&
                        observed.inputs.f1_to_f4 == wanted.f1_to_f4 &&
                        observed.outgoing_stack_84 == 0, "independent call contract");
            }
            require(readable == expected, "independent byte contract");
            ++count;
        };
        for (auto flag : {0u, 1u, 2u, 0x80000000u, 0xFFFFFFFFu})
            for (uint32_t width : {0u, 1u, 1280u, 0xFFFFFFFFu})
                for (uint32_t height : {0u, 1u, 720u, 0xFFFFFFFFu}) {
                    Inputs82303138 in{};
                    for (auto& word : in.r3_to_r10) word = rng();
                    in.r3_to_r10[5] = (rng() & 0xFFFFFFFF00000000ULL) | width;
                    in.r3_to_r10[6] = (rng() & 0xFFFFFFFF00000000ULL) | height;
                    in.f1_to_f4 = {0x8000000000000000, 0x7FF8123456789ABC, 1, UINT64_MAX};
                    check(flag, in);
                }
        for (unsigned i = 0; i < 10000; ++i) {
            Inputs82303138 in{};
            for (auto& word : in.r3_to_r10) word = rng();
            for (auto& word : in.f1_to_f4) word = rng();
            check(i % 2 ? uint32_t(rng()) | 1u : 0u, in);
        }
        std::cout << "{\"cases\":" << count << ",\"delegated\":" << delegated
                  << ",\"writes_only\":" << writes_only << ",\"mismatches\":0}\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
