#pragma once
#include <imgui.h>
#include "neural_scale_policy.hpp"
#include "neural_pass_controls.hpp"

struct NeuralResolveControls { int mode = 1, transfer = 100, color = 100; };
struct NeuralPerformanceEdits { bool pending_changed, apply, sharpness_changed, resolve_changed; };

inline constexpr char neural_color_warning[] =
    "Values above 100% exaggerate neural chroma and may cause oversaturation, out-of-gamut colors, or stronger haloing.";

inline bool draw_slider_reset_context_menu(int &value, int default_value)
{
    if (!ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight)) return false;
    const bool reset = ImGui::Selectable("Reset", false, ImGuiSelectableFlags_None, ImVec2());
    ImGui::EndPopup();
    if (reset) value = default_value;
    return reset;
}

inline bool draw_neural_color_strength(int &color)
{
    bool changed = ImGui::SliderInt("Neural Color Strength", &color, 0,
        nr::maximum_color_percent, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    const bool hovered = ImGui::IsItemHovered();
    changed |= draw_slider_reset_context_menu(color, 100);
    if (hovered) ImGui::SetTooltip("%s", neural_color_warning);
    return changed;
}

inline bool draw_multipass_motion_mode(int &mode)
{
    return ImGui::Combo("Multipass Motion", &mode,
        "Reuse Game Motion (Recommended)\0Zero Later-Pass Motion\0Zero Motion + Reset History\0");
}

inline NeuralPerformanceEdits draw_neural_performance_section(
    bool available, int &pending, int applied, NeuralResolveControls *resolve = nullptr, int effective = 0)
{
    ImGui::SeparatorText("Neural Rendering Performance");
    ImGui::BeginDisabled(!available);
    NeuralPerformanceEdits edits = {};
    edits.pending_changed = ImGui::SliderInt("Neural Rendering Resolution", &pending,
        nr::minimum_scale_percent, nr::maximum_scale_percent, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    edits.pending_changed |= draw_slider_reset_context_menu(pending, nr::native_scale_percent);
    edits.apply = ImGui::Button("Apply##NeuralRenderingResolution") && pending != applied;
    ImGui::SameLine();
    ImGui::Text("Applied: %d%%", applied);
    if (effective >= nr::minimum_scale_percent && effective != applied)
        ImGui::Text("Effective: %d%% (fallback or transition)", effective);
    ImGui::BeginDisabled(applied == nr::native_scale_percent);
    if (resolve)
    {
        edits.resolve_changed = ImGui::Combo("Reconstruction Mode", &resolve->mode,
            "Direct Reconstruction\0Matched Residual\0");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::TextWrapped("Resolution changes NR's internal size. At 100%% the first pass uses direct reconstruction; detail and colour controls above remain available.");
    return edits;
}

inline bool draw_neural_detail_section(bool available, NeuralResolveControls &controls, int &sharpness)
{
    ImGui::SeparatorText("Neural Detail and Colour");
    ImGui::BeginDisabled(!available);
    bool changed = ImGui::SliderInt("Neural Transfer Strength", &controls.transfer, 0, 200,
        "%d%%", ImGuiSliderFlags_AlwaysClamp);
    changed |= draw_slider_reset_context_menu(controls.transfer, 100);
    changed |= draw_neural_color_strength(controls.color);
    bool sharpness_changed = ImGui::SliderInt("Neural Sharpness", &sharpness, 0, 100,
        "%d%%", ImGuiSliderFlags_AlwaysClamp);
    sharpness_changed |= draw_slider_reset_context_menu(sharpness, 0);
    changed |= sharpness_changed;
    ImGui::TextWrapped("These controls affect Pass 1 only, at any NR resolution. Additional passes have their own controls below. Transfer 100%%, colour 100%% and sharpness 0%% preserve the native first pass at 100%% resolution.");
    ImGui::EndDisabled();
    if (!available) ImGui::TextWrapped("Neural detail and colour controls require the DX12 NR working path.");
    return changed;
}

inline bool draw_neural_pass_sections(bool available, unsigned count, nr::PassControls &controls,
                                     nr::PassResolve /*base*/, nr::PassSectionState &sections,
                                     void (*write)(const char *, const char *, int) = nullptr)
{
    ImGui::BeginDisabled(!available);
    bool changed = false;
    const auto remember = [&](unsigned index, bool open) {
        if (sections.expanded[index] != open) {
            sections.expanded[index] = open;
            if (write) write(nr::pass_section_names[index], "Expanded", open ? 1 : 0);
        }
        return open;
    };
    ImGui::SetNextItemOpen(sections.expanded[0], ImGuiCond_Always);
    if (remember(0, ImGui::CollapsingHeader("Per-Pass Controls"))) {
        ImGui::TextWrapped("Pass 1 uses the controls above. Each additional pass has independent controls and processes the previous pass's result.");
        for (unsigned i = 1; i < std::clamp(count, 1u, 10u); ++i) {
            ImGui::PushID(static_cast<int>(i));
            constexpr const char *labels[] = {"Pass 2", "Pass 3", "Pass 4", "Pass 5", "Pass 6", "Pass 7", "Pass 8", "Pass 9", "Pass 10"};
            ImGui::SetNextItemOpen(sections.expanded[i], ImGuiCond_Always);
            if (remember(i, ImGui::TreeNode(labels[i - 1]))) {
                auto &pass = controls.extra[i - 1];
                changed |= ImGui::Combo("Reconstruction Mode", &pass.mode, "Direct Reconstruction\0Matched Residual\0");
                changed |= ImGui::SliderInt("Neural Transfer Strength", &pass.transfer, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                changed |= draw_slider_reset_context_menu(pass.transfer, 100);
                changed |= draw_neural_color_strength(pass.color);
                changed |= ImGui::SliderInt("Neural Sharpness", &pass.sharpness, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                changed |= draw_slider_reset_context_menu(pass.sharpness, 0);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndDisabled();
    if (!available) ImGui::TextWrapped("Additional detail/pass controls require the DX12 NR working path.");
    return changed;
}
