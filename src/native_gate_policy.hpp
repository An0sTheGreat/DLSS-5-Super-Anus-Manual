#pragma once
#include <cstdint>

inline bool permit_native_evaluation(bool original_allowed, bool,
                                     bool, std::uint8_t, bool)
{
    // V6.3's replay bypass could repeat NR and alter the output. A saved legacy
    // toggle must never override upstream duplicate/stale-frame rejection.
    return original_allowed;
}

inline bool use_native_sr_source(unsigned original_method, bool neural_rendering_enabled)
{
    return original_method == 2 || (original_method == 3 && neural_rendering_enabled);
}
