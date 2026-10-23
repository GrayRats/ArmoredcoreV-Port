#pragma once
#include "host.hpp"

// Exact generated declaration: ppc_recomp_shared.h:204080.
PPC_EXTERN_FUNC(__imp__KeQueryPerformanceFrequency);

namespace acv_probe {
// Explicit test-host policy. No default frequency or Xbox threading semantics.
inline thread_local const uint32_t* active_frequency = nullptr;
class FrequencyScope {
    uint32_t frequency_;
    const uint32_t* previous_;
public:
    explicit FrequencyScope(uint32_t frequency)
        : frequency_(frequency), previous_(active_frequency) { active_frequency = &frequency_; }
    ~FrequencyScope() { active_frequency = previous_; }
    FrequencyScope(const FrequencyScope&) = delete;
    FrequencyScope& operator=(const FrequencyScope&) = delete;
};
}
