// Compiled only when ofxKit is available (kit apps); empty TU otherwise.
#if __has_include("Runtime.h")

#include "TanimEditorSystem.h"

#include "ofMain.h"

#if __has_include("tanim/tanim_user.hpp")
#include "tanim/tanim_user.hpp"
#define OFX_ANIMATIONKIT_HAS_TANIM 1
#endif

namespace ofxAnimationKit {

void TanimEditorSystem::setup(entt::registry& /*registry*/)
{
#if OFX_ANIMATIONKIT_HAS_TANIM
    if (!m_tanimInitialized) {
        tanim::Init();
        m_tanimInitialized = true;
    }
#endif
}

void TanimEditorSystem::draw(entt::registry& /*registry*/)
{
#if OFX_ANIMATIONKIT_HAS_TANIM
    if (!m_tanimInitialized) {
        tanim::Init();
        m_tanimInitialized = true;
    }
    tanim::UpdateEditor(ofGetLastFrameTime());
#endif
}

} // namespace ofxAnimationKit

#endif // __has_include("Runtime.h")
