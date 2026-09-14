#pragma once
#include <algorithm>
#include <array>
#include "neural_scale_policy.hpp"

namespace nr {
inline constexpr const char *pass_section_names[] = {"RenoDXPassControls", "RenoDXPass2",
    "RenoDXPass3", "RenoDXPass4", "RenoDXPass5", "RenoDXPass6", "RenoDXPass7",
    "RenoDXPass8", "RenoDXPass9", "RenoDXPass10"};
struct PassSectionState {
    std::array<bool, 10> expanded{true,true,true,true,true,true,true,true,true,true};
};
template<class Read> void load_pass_sections(PassSectionState &state, Read read) {
    for (unsigned i = 0; i < state.expanded.size(); ++i) {
        int value = 1;
        read(pass_section_names[i], "Expanded", value);
        state.expanded[i] = value != 0; // Missing/invalid values default to expanded.
    }
}
struct PassResolve {
    bool enabled = false;
    int mode = 1, transfer = 100, color = 100, sharpness = 0;
    int detail = 100, coupling = 0;
};
struct PassControls {
    int detail = 100, coupling = 0;
    std::array<PassResolve, 9> extra{};
};
inline void clamp_pass_controls(PassControls &controls) {
    controls.detail = 100;
    controls.coupling = 0;
    for (auto &pass : controls.extra) {
        pass.mode = std::clamp(pass.mode, 0, 1);
        pass.transfer = std::clamp(pass.transfer, 0, 200);
        pass.color = std::clamp(pass.color, 0, maximum_color_percent);
        pass.sharpness = std::clamp(pass.sharpness, 0, 100);
        pass.detail = 100;
        pass.coupling = 0;
    }
}
inline PassResolve resolve_pass(const PassControls &controls, unsigned index, PassResolve inherited) {
    if (index > 0 && index <= controls.extra.size())
        inherited = controls.extra[index - 1];
    inherited.detail = 100;
    inherited.coupling = 0;
    return inherited;
}
// Extra passes have independent defaults. Enabled is retained only to migrate
// old presets: previously disabled overrides were inactive, not user choices.
template<class Read> void load_pass_controls(PassControls &controls, Read read) {
    // Retired global detail/coupling must not leave an invisible adjustment.
    controls.detail = 100;
    controls.coupling = 0;
    for (unsigned i = 0; i < controls.extra.size(); ++i) {
        const char *sections[] = {"RenoDXPass2", "RenoDXPass3", "RenoDXPass4", "RenoDXPass5",
            "RenoDXPass6", "RenoDXPass7", "RenoDXPass8", "RenoDXPass9", "RenoDXPass10"};
        auto &pass = controls.extra[i];
        int enabled = 0;
        read(sections[i], "Enabled", enabled); pass.enabled = enabled == 1;
        if (!pass.enabled) { pass = {}; continue; }
        read(sections[i], "Mode", pass.mode); read(sections[i], "TransferPercent", pass.transfer);
        read(sections[i], "ColorPercent", pass.color); read(sections[i], "SharpnessPercent", pass.sharpness);
    }
    clamp_pass_controls(controls);
}
template<class Write> void save_pass_controls(const PassControls &controls, Write write) {
    for (unsigned i = 0; i < controls.extra.size(); ++i) {
        const char *sections[] = {"RenoDXPass2", "RenoDXPass3", "RenoDXPass4", "RenoDXPass5",
            "RenoDXPass6", "RenoDXPass7", "RenoDXPass8", "RenoDXPass9", "RenoDXPass10"};
        const auto &pass = controls.extra[i];
        write(sections[i], "Enabled", 1);
        write(sections[i], "Mode", pass.mode); write(sections[i], "TransferPercent", pass.transfer);
        write(sections[i], "ColorPercent", pass.color); write(sections[i], "SharpnessPercent", pass.sharpness);
    }
}
}
