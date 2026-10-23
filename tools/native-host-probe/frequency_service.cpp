#include "frequency_service.hpp"

// Import boundary at retail thunk 0x83536CA4, xboxkrnl ordinal 0x83.
// Handwritten host adapter, not a recovered retail service implementation.
// u32 result policy follows reference runtime xboxkrnl_threading.cpp:371-374;
// the actual ACV/Xenon frequency remains unverified.
PPC_FUNC(__imp__KeQueryPerformanceFrequency) {
    if (!acv_probe::active_frequency) throw std::logic_error("no configured guest frequency");
    ctx.r3.u64 = *acv_probe::active_frequency;
}
