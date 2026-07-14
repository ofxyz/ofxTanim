#pragma once

#include <entt.hpp>
#include <string>
#include <unordered_map>

namespace ofxAnimationKit {

/// uid → entity map used by tanim::FindEntityOfUID when this bridge is active.
class AnimationBridge {
public:
    void bindRegistry(entt::registry& registry);
    entt::registry* registry() const { return m_registry; }

    void registerUid(const std::string& uid, entt::entity entity);
    void unregisterUid(const std::string& uid);
    void unregisterEntity(entt::entity entity);
    void clear();

    entt::entity entityOfUid(const std::string& uid) const;
    bool hasUid(const std::string& uid) const;

    /// Install this bridge as the process-wide fallback for tanim::FindEntityOfUID.
    void installAsTanimDefault();

private:
    entt::registry* m_registry {nullptr};
    std::unordered_map<std::string, entt::entity> m_uidToEntity;
    std::unordered_map<entt::entity, std::string> m_entityToUid;
};

} // namespace ofxAnimationKit
