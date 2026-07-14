#pragma once

// ============================================================================
// ofxAnimationKit — ofxTanim bridge for ofxKit runtime
// (ships inside ofxTanim under src/kit/, like ofxBulletKit.h in ofxBullet)
// ============================================================================
// Requires ofxKit + ofxEnTTKit in addons.make. Standalone ofxTanim apps
// should not include this header.
//
//   #include "kit/ofxAnimationKit.h"
//
//   ofxAnimationKit::AnimationKit m_animKit;
//   m_animKit.registerWith(ofkitty::runtime());
// ============================================================================

#include "AnimationBridge.h"
#include "TanimEditorSystem.h"
#include "Runtime.h"
#include "ViewWindow.h"

namespace ofxAnimationKit {

class AnimationKit {
public:
    struct Options {
        bool registerTimelineWindow {true};
        bool windowVisible          {false};
        bool installTanimHooks      {true};
        bool registerEditorSystem   {true}; ///< Gui-phase tanim::UpdateEditor system
    };

    // Two overloads instead of a default argument — GCC rejects nested-class
    // NSDMI defaults in enclosing-class member declarations.
    void registerWith(ofkitty::Runtime& runtime);
    void registerWith(ofkitty::Runtime& runtime, const Options& opts);

    AnimationBridge&       bridge()       { return m_bridge; }
    const AnimationBridge& bridge() const { return m_bridge; }

    bool tanimInitialized() const { return m_tanimInitialized; }

private:
    void ensureTanimInitialized();
    void drawTimelineWindow(bool& visible);

    AnimationBridge    m_bridge;
    TanimEditorSystem  m_editorSystem;
    bool               m_tanimInitialized {false};
    bool               m_registered       {false};
    bool               m_systemRegistered {false};
};

} // namespace ofxAnimationKit
