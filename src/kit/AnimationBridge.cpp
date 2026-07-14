// Compiled only when ofxKit is available (kit apps). Standalone ofxTanim
// builds (example-basic / example-minimal) define their own
// tanim::FindEntityOfUID / LogError / LogInfo, so this TU must stay empty
// there to avoid duplicate symbols.
#if __has_include("Runtime.h")

#include "AnimationBridge.h"

#include "ofLog.h"

#if __has_include("tanim/user_data.hpp")
#include "tanim/user_data.hpp"
#include "tanim/user_override.hpp"
#define OFX_ANIMATIONKIT_HAS_TANIM 1
#endif

namespace ofxAnimationKit {

namespace {

AnimationBridge* g_defaultBridge = nullptr;

entt::entity resolveUid(AnimationBridge* bridge, const std::string& uid)
{
    if (!bridge || uid.empty())
        return entt::null;

    const entt::entity entity = bridge->entityOfUid(uid);
    if (entity == entt::null)
        return entt::null;

    if (bridge->registry() && !bridge->registry()->valid(entity))
        return entt::null;

    return entity;
}

} // namespace

void AnimationBridge::bindRegistry(entt::registry& registry)
{
    m_registry = &registry;
}

void AnimationBridge::registerUid(const std::string& uid, entt::entity entity)
{
    if (uid.empty() || entity == entt::null)
        return;

    if (auto it = m_entityToUid.find(entity); it != m_entityToUid.end())
        m_uidToEntity.erase(it->second);

    m_uidToEntity[uid] = entity;
    m_entityToUid[entity] = uid;
}

void AnimationBridge::unregisterUid(const std::string& uid)
{
    const auto it = m_uidToEntity.find(uid);
    if (it == m_uidToEntity.end())
        return;

    m_entityToUid.erase(it->second);
    m_uidToEntity.erase(it);
}

void AnimationBridge::unregisterEntity(entt::entity entity)
{
    const auto it = m_entityToUid.find(entity);
    if (it == m_entityToUid.end())
        return;

    m_uidToEntity.erase(it->second);
    m_entityToUid.erase(it);
}

void AnimationBridge::clear()
{
    m_uidToEntity.clear();
    m_entityToUid.clear();
}

entt::entity AnimationBridge::entityOfUid(const std::string& uid) const
{
    const auto it = m_uidToEntity.find(uid);
    return it != m_uidToEntity.end() ? it->second : entt::null;
}

bool AnimationBridge::hasUid(const std::string& uid) const
{
    return m_uidToEntity.find(uid) != m_uidToEntity.end();
}

void AnimationBridge::installAsTanimDefault()
{
    g_defaultBridge = this;
}

} // namespace ofxAnimationKit

#if OFX_ANIMATIONKIT_HAS_TANIM

namespace tanim {

entt::entity FindEntityOfUID(const ComponentData& /*cdata*/, const std::string& uid_to_find)
{
    using namespace ofxAnimationKit;
    if (g_defaultBridge)
        return resolveUid(g_defaultBridge, uid_to_find);
    return entt::null;
}

void LogError(const std::string& message)
{
    ofLogError("Tanim") << message;
}

void LogInfo(const std::string& message)
{
    ofLogNotice("Tanim") << message;
}

} // namespace tanim

#endif // OFX_ANIMATIONKIT_HAS_TANIM

#endif // __has_include("Runtime.h")
