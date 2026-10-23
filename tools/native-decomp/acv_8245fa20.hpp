#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace acv_decomp {
// Field names describe observed operations, not recovered retail symbols.
struct TimerState {
    uint8_t active;
    uint8_t mode;
    uint16_t counter;
    float period;
    float accumulator;
};
static_assert(offsetof(TimerState, active) == 0 && offsetof(TimerState, mode) == 1);
static_assert(offsetof(TimerState, counter) == 2 && offsetof(TimerState, period) == 4);
static_assert(offsetof(TimerState, accumulator) == 8 && sizeof(TimerState) == 12);

// @retail 0x8245FA20
// High confidence state reconstruction from 48 instructions [0x8245FA20,0x8245FAE0).
// No .pdata owner: bounded complete leaf, not a .pdata-confirmed function.
// State-only contract: excludes scratch stack, volatile registers, CR/XER/FPSCR.
// The f1 input is double: do not pre-round it to float before adding old state.
inline void advance_timer(TimerState& state, double elapsed_seconds) {
    if (state.active == 0) return;
    state.accumulator = float(elapsed_seconds + double(state.accumulator));
    switch (state.mode) {
    case 1:
    case 3:
        // fcmpu + bltlr returns only when LT, including unordered fallthrough.
        if (!(state.accumulator < state.period)) {
            if (state.counter != std::numeric_limits<uint16_t>::max()) ++state.counter;
            // Exactly one period is removed, regardless of overshoot.
            state.accumulator = float(double(state.accumulator) - double(state.period));
        }
        break;
    case 2:
    case 4:
        if (!(state.accumulator < state.period)) state.active = 0;
        break;
    default:
        // Unknown modes still accumulate because dispatch follows fadds/stfs.
        break;
    }
}
} // namespace acv_decomp
