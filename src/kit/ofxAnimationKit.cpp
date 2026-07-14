// Compiled only when ofxKit is available (kit apps); empty TU otherwise so
// standalone ofxTanim builds don't need the ofxKit runtime.
#if __has_include("Runtime.h")

#include "ofxAnimationKit.h"

#include "imgui.h"

#if __has_include("tanim/tanim_user.hpp")
#include "tanim/tanim_user.hpp"
#define OFX_ANIMATIONKIT_HAS_TANIM 1
#endif

namespace ofxAnimationKit {

void AnimationKit::ensureTanimInitialized()
{
#if OFX_ANIMATIONKIT_HAS_TANIM
    if (!m_tanimInitialized) {
        tanim::Init();
        m_tanimInitialized = true;
    }
#else
    m_tanimInitialized = false;
#endif
}

void AnimationKit::drawTimelineWindow(bool& visible)
{
    if (!ImGui::Begin("Timeline", &visible)) {
        ImGui::End();
        return;
    }

#if OFX_ANIMATIONKIT_HAS_TANIM
    ensureTanimInitialized();
    ImGui::TextUnformatted("ofxTanim editor (placeholder host).");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Call tanim::OpenForEditing() from your app to bind a timeline. "
        "This window will host tanim::Draw() in a future revision.");
    if (m_tanimInitialized) {
        ImGui::TextDisabled("tanim::Init() — ok");
    }
#else
    ImGui::TextWrapped(
        "ofxTanim headers not found at compile time. "
        "Add ofxTanim to addons.make to enable the timeline editor.");
#endif

    ImGui::End();
}

void AnimationKit::registerWith(ofkitty::Runtime& runtime, const Options& opts)
{
    m_bridge.bindRegistry(runtime.registry());

    if (opts.installTanimHooks)
        m_bridge.installAsTanimDefault();

    if (opts.registerTimelineWindow
        && !runtime.findWindow("Timeline")) {
        const bool vis = opts.windowVisible;
        runtime.registerWindow(ofkitty::makeViewWindow(
            "Timeline",
            [this](bool& visible) { drawTimelineWindow(visible); },
            {.visible = vis, .id = "ofxanimationkit.window.timeline"}));
    }

    m_registered = true;
}

} // namespace ofxAnimationKit

#endif // __has_include("Runtime.h")
