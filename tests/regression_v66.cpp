#define NOMINMAX
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <imgui_internal.h>
#include "neural_ui_layout.hpp"
#include "runtime_api_ui.hpp"
#include "debug_section_default.hpp"
#include "neural_performance_ui.hpp"
#include "neural_scale_policy.hpp"

static void test_backend_status()
{
    using namespace nr::backends;
    using reshade::api::device_api;
#ifdef NR_DX11_GAME_TEST
    assert(!support(device_api::d3d11).evaluator_available); // Never DX12 resource dispatch.
    assert(!support(device_api::vulkan).evaluator_available);
    assert(std::strstr(runtime_status(device_api::d3d11, false, true).evaluation_state,
                       "DX11 native SR bridge"));
#endif
    EvaluationDevice device;
    assert(!device.observed());
    device.observe(0, 1);
    device.observe(1, 0);
    device.observe(1, 0x100); // Wrapper success is the low byte, not the full word.
    assert(!device.observed());
    device.observe(1, 1);
    assert(device.observed());
    device.forget(2); // Teardown of an unrelated device must not erase the state.
    assert(device.observed());
    device.observe(2, 1);
    device.forget(1);
    assert(device.observed());
    device.forget(2);
    assert(!device.observed());

    for (auto api : {device_api::d3d12, device_api::d3d11, device_api::d3d9,
                    device_api::vulkan, device_api::opengl, static_cast<device_api>(0)})
    {
        for (bool observed : {false, true})
        {
            const auto status = runtime_status(api, observed, true);
            assert(status.controls_available == (api == device_api::d3d12 || observed));
            assert(!runtime_status(api, observed, false).controls_available);
        }
    }
}

static void test_pass_controls()
{
    nr::PassControls controls;
    const nr::PassResolve inherited{false,0,73,64,19};
    for (unsigned index = 0; index < 12; ++index) {
        const auto pass = nr::resolve_pass(controls,index,inherited);
        if (index == 0 || index > 9)
            assert(pass.mode == 0 && pass.transfer == 73 && pass.color == 64 && pass.sharpness == 19);
        else assert(pass.mode == 1 && pass.transfer == 100 && pass.color == 100 && pass.sharpness == 0);
        assert(pass.detail == 100 && pass.coupling == 0);
    }
    controls.extra[0] = {true,1,120,43,7,170,25};
    controls.detail = 130; controls.coupling = 200;
    assert(nr::resolve_pass(controls,0,inherited).detail == 100);
    assert(nr::resolve_pass(controls,1,inherited).detail == 100);
    assert(nr::resolve_pass(controls,2,inherited).detail == 100);
    assert(nr::resolve_pass(controls,1,{false,0,0,0,0}).transfer == 120);
    assert(nr::resolve_pass(controls,0,inherited).transfer == 73);
    std::map<std::string,int> config;
    nr::save_pass_controls(controls,[&](const char *section,const char *key,int value) {
        config[std::string(section)+"/"+key] = value;
    });
    nr::PassControls loaded;
    auto read = [&](const char *section,const char *key,int &value) {
        const auto it = config.find(std::string(section)+"/"+key);
        if (it != config.end()) value = it->second;
    };
    nr::load_pass_controls(loaded,read);
    assert(loaded.detail == 100 && loaded.coupling == 0 && loaded.extra[0].enabled);
    assert(loaded.extra[0].transfer == 120 && loaded.extra[0].detail == 100 && loaded.extra[1].enabled);
    config["RenoDXNeuralDetail/DetailPercent"] = 170;
    config["RenoDXNeuralDetail/CouplingPercent"] = 250;
    config["RenoDXPass2/CouplingPercent"] = 999999;
    config["RenoDXPass2/Enabled"] = -1;
    nr::load_pass_controls(loaded,read);
    assert(loaded.detail == 100 && loaded.coupling == 0 && loaded.extra[0].coupling == 0 && !loaded.extra[0].enabled);
    assert(loaded.extra[0].color == 100 && loaded.extra[0].sharpness == 0);
    config.clear(); loaded = {}; nr::load_pass_controls(loaded,read);
    assert(loaded.detail == 100 && loaded.coupling == 0 && !loaded.extra[0].enabled);
    assert(loaded.extra[0].color == 100);
    loaded.extra[0].color = 999; nr::clamp_pass_controls(loaded);
    assert(loaded.extra[0].color == nr::maximum_color_percent);
    loaded.extra[0].color = -1; nr::clamp_pass_controls(loaded);
    assert(loaded.extra[0].color == 0);
    assert(std::strstr(neural_color_warning, "above 100%") &&
        std::strstr(neural_color_warning, "haloing"));
    for (unsigned count = 1; count <= 10; ++count) for (bool open : {false,true}) {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1000,2000); io.DeltaTime = 1.f/60.f;
        unsigned char *pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ImGui::NewFrame(); ImGui::SetNextWindowSize(ImVec2(980,1900)); ImGui::Begin("Pass test");
        nr::PassSectionState sections;
        for (bool expanded : sections.expanded) assert(expanded);
        sections.expanded[0] = open;
        const int depth = ImGui::GetCurrentWindow()->DC.TreeDepth;
        ImGui::LogToBuffer(0);
        assert(!draw_neural_pass_sections(true,count,loaded,inherited,sections));
        assert(ImGui::GetCurrentWindow()->DC.TreeDepth == depth);
        const char *log = ImGui::GetCurrentContext()->LogBuffer.c_str();
        assert(!std::strstr(log,"Hue-stable Detail Strength") && !std::strstr(log,"Colour Coupling"));
        assert(!std::strstr(log,"Customize this pass"));
        for (unsigned p = 2; p <= 10; ++p) {
            char name[16]; std::snprintf(name,sizeof(name),"Pass %u",p);
            assert((std::strstr(log,name) != nullptr) == (open && p <= count));
        }
        assert(loaded.detail == 100 && !loaded.extra[0].enabled);
        ImGui::LogFinish(); ImGui::End(); ImGui::Render(); ImGui::DestroyContext();
    }
    std::puts("Pass controls: independent defaults/isolation, retired controls ignored, per-pass config round-trip, 1-10 pass visibility and expanded defaults passed.");
}

static std::map<std::string,int> section_config;
static void write_section(const char *section, const char *key, int value) {
    section_config[std::string(section) + "/" + key] = value;
}
static void test_section_persistence()
{
    for (unsigned target_index = 0; target_index < 10; ++target_index) {
        section_config.clear();
        nr::PassSectionState state;
        state.expanded.fill(false);
        if (target_index) state.expanded[0] = true;
        nr::PassControls controls;
        ImGui::CreateContext();
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1000,2000); io.DeltaTime = 1.f/60.f;
        unsigned char *pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ImVec2 target;
        for (int frame = 0; frame < 6; ++frame) {
            if (frame) { io.AddMousePosEvent(target.x,target.y); io.AddMouseButtonEvent(0,frame == 1 || frame == 4); }
            ImGui::NewFrame(); ImGui::SetNextWindowPos(ImVec2(20,20));
            ImGui::SetNextWindowSize(ImVec2(980,1900)); ImGui::Begin("Section persistence");
            assert(!draw_neural_pass_sections(true,target_index + 1,controls,{},state,&write_section));
            if (frame == 0) target = ImVec2(ImGui::GetItemRectMin().x + 8, ImGui::GetItemRectMin().y + 8);
            if (frame == 2) assert(state.expanded[target_index]);
            if (frame == 5) assert(!state.expanded[target_index]);
            ImGui::End(); ImGui::Render();
        }
        ImGui::DestroyContext();
        assert(section_config.size() == 1);
        nr::PassSectionState restored;
        nr::load_pass_sections(restored,[](const char *section,const char *key,int &value) {
            const auto it = section_config.find(std::string(section) + "/" + key);
            if (it != section_config.end()) value = it->second;
        });
        for (unsigned i = 0; i < 10; ++i) assert(restored.expanded[i] == (i != target_index));
        // Recreate ImGui as on restart: stored closed state wins over its empty storage.
        ImGui::CreateContext();
        auto &fresh = ImGui::GetIO(); fresh.IniFilename = nullptr; fresh.LogFilename = nullptr;
        fresh.DisplaySize = ImVec2(1000,600); fresh.DeltaTime = 1.f/60.f;
        fresh.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ImGui::NewFrame(); ImGui::Begin("Section persistence");
        draw_neural_pass_sections(true,1,controls,{},restored,&write_section);
        assert(!restored.expanded[target_index]); // Hiding a pass doesn't erase its choice.
        ImGui::End(); ImGui::Render(); ImGui::DestroyContext();
    }
    std::puts("Section persistence: all ten headers toggle, save, restore and retain hidden-pass state; unseen sections default expanded.");
}

static void test_layout(float width, float font_scale, bool advanced_open,
                        bool debug_open, bool runtime_open, bool observed)
{
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(3440, 1440); io.DeltaTime = 1.0f / 60.0f;
    io.FontGlobalScale = font_scale;
    unsigned char *pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    for (int frame = 0; frame < 3; ++frame)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(57, 32));
        ImGui::SetNextWindowSize(ImVec2(width, 1326));
        ImGui::Begin("RenoDX DLSS_A", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::LogToBuffer(0);
        ImGui::TextUnformatted("Reference setting");
        const float root_x = ImGui::GetItemRectMin().x;
        const auto status = nr::backends::runtime_status(reshade::api::device_api::d3d11, observed, true);
        const float alpha = ImGui::GetStyle().Alpha;
        const int depth = ImGui::GetCurrentWindow()->DC.TreeDepth;
        nr::PassControls pass_controls;
        nr::PassSectionState sections;
        sections.expanded[0] = false;
        int pending = 75, sharpness = 25;
        NeuralResolveControls resolve;
        begin_neural_controls_layout();
        int motion_mode = static_cast<int>(nr::MultipassMotionMode::reuse_game_motion);
        assert(!draw_multipass_motion_mode(motion_mode));
        assert(!draw_neural_detail_section(status.controls_available, resolve, sharpness));
        assert(ImGui::GetStyle().Alpha == alpha && ImGui::GetCurrentWindow()->DC.TreeDepth == depth);
        assert(std::fabs(ImGui::GetItemRectMin().x - root_x) < 0.1f);
        const auto edits = draw_neural_performance_section(status.controls_available, pending, 100, &resolve);
        assert(!edits.pending_changed && !edits.apply && !edits.sharpness_changed);
        assert(pending == 75 && sharpness == 25);
        assert(std::fabs(ImGui::GetItemRectMin().x - root_x) < 0.1f);
        assert(ImGui::GetStyle().Alpha == alpha && ImGui::GetCurrentWindow()->DC.TreeDepth == depth);
        ImGui::EndGroup();
        ImGui::SetNextItemOpen(advanced_open, ImGuiCond_Always);
        if (ImGui::TreeNode("Advanced")) { ImGui::TextUnformatted("Pass Count"); ImGui::TreePop(); }
        const float advanced_end = ImGui::GetCursorPosY();
        begin_neural_controls_layout();
        draw_neural_pass_sections(status.controls_available, 3, pass_controls, {}, sections);
        assert(ImGui::GetCursorPosY() > advanced_end);
        ImGui::SeparatorText("Controls");
        assert(std::fabs(ImGui::GetItemRectMin().x - root_x) < 0.1f);
        ImGui::TextUnformatted("Numpad / and *");
        assert((ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) == 0);
        assert(ImGui::GetStyle().Alpha == alpha && ImGui::GetCurrentWindow()->DC.TreeDepth == depth);
        ImGui::EndGroup();
        const float performance_end = ImGui::GetCursorPosY();
        apply_debug_section_default("Debug", 5);
        ImGui::SetNextItemOpen(debug_open, ImGuiCond_Always);
        if (ImGui::TreeNodeEx("Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("Debug counters");
            ImGui::TreePop(); ImGui::Unindent(); // reproduce the stock bad indent
        }
        assert(ImGui::GetCursorPosY() > performance_end);
        const float debug_end = ImGui::GetCursorPosY();
        begin_neural_controls_layout();
        ImGui::SetNextItemOpen(runtime_open, ImGuiCond_Always);
        draw_runtime_api_section(status);
        assert(ImGui::GetCursorPosY() > debug_end);
        assert(ImGui::GetStyle().Alpha == alpha && ImGui::GetCurrentWindow()->DC.TreeDepth == depth);
        ImGui::EndGroup();
        apply_debug_section_default("Links", 5);
        if (ImGui::TreeNodeEx("Links", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Button("Discord"); ImGui::TreePop();
        }
        assert(std::fabs(ImGui::GetItemRectMin().x - root_x) < 0.1f);
        apply_debug_section_default("About", 5);
        if (ImGui::TreeNodeEx("About", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextUnformatted("Build: V1.0.3 | D: 2026-09-12 | T: 15:14:38"); ImGui::TreePop();
        }
        assert(std::fabs(ImGui::GetItemRectMin().x - root_x) < 0.1f);
        const char *log = ImGui::GetCurrentContext()->LogBuffer.c_str();
        const char *advanced = std::strstr(log, "Advanced");
        const char *performance = std::strstr(log, "Neural Rendering Performance");
        const char *debug = std::strstr(log, "Debug");
        const char *runtime = std::strstr(log, "Runtime API");
        const char *controls = std::strstr(log, "Numpad / and *");
        assert(advanced && performance && debug && runtime && controls);
        assert(performance < advanced && controls < debug && debug < runtime);
        const char *links = std::strstr(log, "Links"), *about = std::strstr(log, "About");
        assert(links && about && runtime < links && links < about);
        const char *detail = std::strstr(log, "Neural Detail and Colour");
        const char *passes = std::strstr(log, "Per-Pass Controls");
        const char *motion = std::strstr(log, "Multipass Motion");
        assert(motion && detail && passes && motion < detail && detail < performance &&
            advanced < passes && passes < controls);
        for (const char *label : {"Neural Transfer Strength", "Neural Color Strength", "Neural Sharpness"}) {
            const char *found = std::strstr(log,label);
            assert(found && detail < found && found < performance);
            assert(std::strstr(found + 1,label) == nullptr);
        }
        assert(!std::strstr(log,"Hue-stable Detail Strength") && !std::strstr(log,"Colour Coupling"));
        assert(std::strstr(detail + 1, "Neural Detail and Colour") == nullptr);
        assert((std::strstr(log, "Presentation API (last observed): DX11") != nullptr) == runtime_open);
        assert(std::strstr(log, "Applied: 100%"));
        for (const char *removed : {"Moving the slider", "XeFG native-input compatibility",
             "Working textures:", "Capture 10-second", "Integrated DX11 game test:"})
            assert(!std::strstr(log, removed));
        ImGui::LogFinish(); ImGui::End(); ImGui::Render();
    }
    ImGui::DestroyContext();
}

static void test_sharpness_interaction(bool available, int applied, int staged, int control)
{
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 700); io.DeltaTime = 1.f / 60.f;
    unsigned char *pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    int pending = staged, sharpness = 25;
    NeuralResolveControls resolve;
    ImVec2 target;
    bool changed = false;
    // Let the new window's automatic navigation focus settle before clicking.
    for (int frame = 0; frame < 6; ++frame)
    {
        if (frame > 1)
        {
            io.AddMousePosEvent(target.x + (frame >= 3 ? 100.f : 0.f), target.y);
            io.AddMouseButtonEvent(0, frame <= 3);
        }
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(20, 20));
        ImGui::SetNextWindowSize(ImVec2(750, 500));
        ImGui::Begin("Sharpness interaction", nullptr, ImGuiWindowFlags_NoSavedSettings);
        const float root_x = ImGui::GetCursorScreenPos().x;
        const float alpha = ImGui::GetStyle().Alpha;
        changed |= draw_neural_detail_section(available, resolve, sharpness);
        // The final explanation follows the sharpness slider by ItemSpacing.y.
        target = ImVec2(root_x + 300.f, ImGui::GetItemRectMin().y -
            ImGui::GetStyle().ItemSpacing.y - ImGui::GetFrameHeight() * .5f -
            (2 - control) * (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y));
        const auto edits = draw_neural_performance_section(available, pending, applied, &resolve, 100);
        assert(!edits.pending_changed && !edits.apply && pending == staged);
        assert(ImGui::GetStyle().Alpha == alpha);
        assert((ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) == 0);
        ImGui::End(); ImGui::Render();
    }
    const bool enabled = available;
    if (changed != enabled) std::fprintf(stderr,"Control click failed: available=%d applied=%d staged=%d control=%d transfer=%d color=%d sharpness=%d target=%f,%f\n",
        available,applied,staged,control,resolve.transfer,resolve.color,sharpness,target.x,target.y);
    assert(changed == enabled);
    assert((resolve.transfer != 100) == (enabled && control == 0));
    assert((resolve.color != 100) == (enabled && control == 1));
    assert((sharpness != 25) == (enabled && control == 2));
    ImGui::DestroyContext();
}

int main()
{
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; ImGui::GetIO().LogFilename = nullptr;
    ImGui::GetIO().DisplaySize = ImVec2(800, 600);
    unsigned char *pixels; int atlas_width, atlas_height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    for (int frame = 0; frame < 4; ++frame)
    {
        ImGui::NewFrame(); ImGui::Begin("Debug default test");
        apply_debug_section_default("Debug", 5);
        // Emulate a user opening the node after its first appearance.
        if (frame == 1) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        const bool opened = ImGui::TreeNodeEx("Debug", ImGuiTreeNodeFlags_DefaultOpen);
        assert(opened == (frame > 0));
        if (opened) ImGui::TreePop();
        for (const char *section : {"Links", "About"}) {
            apply_debug_section_default(section, 5);
            if (frame == 1) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            const bool section_open = ImGui::TreeNodeEx(section, ImGuiTreeNodeFlags_DefaultOpen);
            assert(section_open == (frame > 0));
            if (section_open) ImGui::TreePop();
        }
        apply_debug_section_default("Advanced", 8);
        assert(ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_DefaultOpen)); ImGui::TreePop();
        if (frame == 1) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        draw_runtime_api_section(nr::backends::runtime_status(reshade::api::device_api::d3d11, true, true));
        assert(ImGui::GetStateStorage()->GetInt(ImGui::GetID("Runtime API"), 0) == (frame > 0));
        ImGui::End(); ImGui::Render();
    }
    ImGui::DestroyContext();
    std::puts("Debug/Runtime API/Links/About: initially collapsed, user expansion retained; Advanced unchanged.");
    test_backend_status();
    test_pass_controls();
    test_section_persistence();
    for (bool available : {false, true})
    for (int applied = 25; applied <= 150; ++applied)
    for (int staged : {25, 99, 100, 101, 150})
    for (int control = 0; control < 3; ++control) test_sharpness_interaction(available, applied, staged, control);
    assert(nr::clamp_scale_percent(0) == 25 && nr::clamp_scale_percent(999) == 150);
    assert(!nr::uses_scaled_path(100) && nr::uses_scaled_path(99) && nr::uses_scaled_path(101));
    assert(!nr::uses_evaluation_working_path(100, 0));
    for (unsigned pass = 1; pass < 10; ++pass) assert(nr::uses_evaluation_working_path(100, pass));
    assert(nr::scaled_extent(3840,150) == 5760 && nr::scaled_extent(2160,25) == 540);
    using nr::MultipassMotionMode;
    for (unsigned pass = 0; pass < 10; ++pass)
    {
        assert(nr::motion_resample_filter(MultipassMotionMode::reuse_game_motion, pass) == 3);
        assert(!nr::reset_later_pass_history(MultipassMotionMode::reuse_game_motion, pass));
    }
    for (unsigned pass = 1; pass < 10; ++pass)
    {
        assert(nr::motion_resample_filter(MultipassMotionMode::zero_later_passes, pass) == 5);
        assert(nr::reset_later_pass_history(MultipassMotionMode::zero_and_reset_later_passes, pass));
    }
    assert(!nr::uses_base_resolve(100,100,0));
    assert(nr::uses_base_resolve(99,100,0) && nr::uses_base_resolve(100,99,0) && nr::uses_base_resolve(100,100,1));
    std::puts("Independent transfer/colour/sharpness: 3780 click cases passed at all 25-150% applied/staged resolutions, including 100%; native-neutral resolve policy passed.");
    for (float width : {500.f, 763.f, 1100.f})
    for (float scale : {1.f, 1.5f, 2.f})
    for (bool advanced : {false, true})
    for (bool debug : {false, true})
    for (bool runtime : {false, true})
    for (bool observed : {false, true}) test_layout(width, scale, advanced, debug, runtime, observed);
    std::puts("V6.6 compact UI: 144 cases, 432 ImGui frames; section order/alignment/default collapse/disabled-state isolation passed.");
}
