#pragma once

// Gui-phase system: keeps tanim::UpdateEditor running while the ImGui frame
// is open (Runtime::drawSystems("gui")). Compile as empty when ofxKit / tanim
// headers are unavailable.

#include "systems/base_system.h"

namespace ofxAnimationKit {

class TanimEditorSystem : public ecs::ISystem {
public:
    const char* getName() const override { return "TanimEditorSystem"; }

    void setup(entt::registry& registry) override;
    void draw(entt::registry& registry) override;

private:
    bool m_tanimInitialized {false};
};

} // namespace ofxAnimationKit
