#pragma once
#include <array>
#include <cstdint>

namespace acv_decomp {
// Opaque incoming register bits: no unproven retail parameter types or names.
struct Inputs82303138 {
    std::array<uint64_t, 8> r3_to_r10{};
    std::array<uint64_t, 4> f1_to_f4{};
};

struct Call82302520 {
    Inputs82303138 inputs;
    uint32_t outgoing_stack_84;
};

// @retail 0x82303138
// Status: partial semantic reconstruction; callee 0x82302520 stays external.
// Confidence: high for branch, object writes and documented call inputs only.
// Memory provides explicit big-endian load32/store32 at uint32 guest addresses.
// Excludes prologue/scratch stack, return/volatile registers, CR/XER/FPSCR.
template<class Memory, class Callee>
void update_82303138(Memory& memory, Inputs82303138 inputs, Callee&& callee) {
    const auto object = static_cast<uint32_t>(inputs.r3_to_r10[0]);
    if (memory.load32(object + uint32_t{0x1D0}) != 0) {
        memory.store32(object + uint32_t{0x4C},
                       static_cast<uint32_t>(inputs.r3_to_r10[5])); // r8
        memory.store32(object + uint32_t{0x50},
                       static_cast<uint32_t>(inputs.r3_to_r10[6])); // r9
        return;
    }
    inputs.r3_to_r10[7] = 0; // li r10,0
    inputs.f1_to_f4[3] = UINT64_C(0x3FF0000000000000); // lfs 0x820018A0: 1.0
    callee(memory, Call82302520{inputs, 0});
}
} // namespace acv_decomp
