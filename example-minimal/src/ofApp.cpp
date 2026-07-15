#include "ofApp.h"

#include <algorithm>
#include <any>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ImHelpers.h"
#include "tanim/registry.hpp"
#include "tanim/sequence_id.hpp"
#include "ComponentInspector.h"
#include <imgui_internal.h>

namespace {

void clearImGuiDockNodeResizeLocks(ImGuiDockNode* node)
{
    if (node == nullptr) return;
    constexpr ImGuiDockNodeFlags kResizeLockMask =
        ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoResizeX | ImGuiDockNodeFlags_NoResizeY;
    node->LocalFlags &= ~kResizeLockMask;
    node->UpdateMergedFlags();
    clearImGuiDockNodeResizeLocks(node->ChildNodes[0]);
    clearImGuiDockNodeResizeLocks(node->ChildNodes[1]);
}

struct DemoAnimComponent {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    bool toggle{true};
    bool bangPulse{false};
};

struct DemoNameComponent {
    std::string uid;
    std::string display;
};

struct TanimUserData {
    entt::registry* registry{nullptr};
    std::unordered_map<std::string, entt::entity> uidToEntity;
};

constexpr int TIMELINE_LAST_FRAME = 240;
constexpr int TIMELINE_MAX_FRAME = 480;

void setOrAddKeyframe(tanim::internal::Sequence& seq, int curveIndex, int frame, float value) {
    const auto keyframeIndex = seq.GetKeyframeIdx(curveIndex, frame);
    if (keyframeIndex.has_value()) {
        seq.EditKeyframe(curveIndex, keyframeIndex.value(), {static_cast<float>(frame), value});
        return;
    }

    seq.AddKeyframeAtPos(curveIndex, {static_cast<float>(frame), value});
    const auto addedIndex = seq.GetKeyframeIdx(curveIndex, frame);
    if (addedIndex.has_value()) {
        seq.EditKeyframe(curveIndex, addedIndex.value(), {static_cast<float>(frame), value});
    }
}

tanim::internal::Sequence* addSequenceByField(entt::registry& registry,
                                               const tanim::EntityData& entityData,
                                               tanim::TimelineData& timelineData,
                                               tanim::ComponentData& componentData,
                                               const std::string& fieldName) {
    const std::size_t beforeCount = timelineData.m_sequences.size();
    const auto& components = tanim::internal::GetRegistry().GetComponents();
    const std::string structName = "DemoAnimComponent";

    for (const auto& component : components) {
        if (component.m_struct_name != structName) {
            continue;
        }

        for (const auto& registeredField : component.m_field_names) {
            if (registeredField != fieldName) {
                continue;
            }

            tanim::internal::SequenceId seqId{entityData, component.m_struct_name, registeredField};
            component.m_add_sequence(registry, timelineData, componentData, seqId);
            if (timelineData.m_sequences.size() > beforeCount) {
                return &timelineData.m_sequences.back();
            }
            return nullptr;
        }
    }

    return nullptr;
}

void configureVec3Sequence(tanim::internal::Sequence& seq, const std::vector<std::pair<int, glm::vec3>>& keys) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    for (const auto& [frame, value] : keys) {
        setOrAddKeyframe(seq, 0, frame, value.x);
        setOrAddKeyframe(seq, 1, frame, value.y);
        setOrAddKeyframe(seq, 2, frame, value.z);
    }
    seq.Fit();
}

void configurePositionSequence(tanim::internal::Sequence& seq) {
    configureVec3Sequence(seq,
                          {
                              {0, {0.0f, 0.0f, 0.0f}},
                              {50, {-180.0f, 85.0f, -70.0f}},
                              {110, {170.0f, -40.0f, 95.0f}},
                              {170, {0.0f, 110.0f, -130.0f}},
                              {TIMELINE_LAST_FRAME, {0.0f, 0.0f, 0.0f}},
                          });
}

void configureRotationSequence(tanim::internal::Sequence& seq) {
    configureVec3Sequence(seq,
                          {
                              {0, {0.0f, 0.0f, 0.0f}},
                              {60, {60.0f, 180.0f, 45.0f}},
                              {120, {180.0f, 420.0f, 120.0f}},
                              {180, {300.0f, 620.0f, 250.0f}},
                              {TIMELINE_LAST_FRAME, {360.0f, 720.0f, 360.0f}},
                          });
}

void configureToggleSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 1.0f);
    setOrAddKeyframe(seq, 0, 48, 0.0f);
    setOrAddKeyframe(seq, 0, 96, 1.0f);
    setOrAddKeyframe(seq, 0, 144, 0.0f);
    setOrAddKeyframe(seq, 0, 192, 1.0f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 1.0f);
    seq.Fit();
}

void configureBangPulseSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 0.0f);
    setOrAddKeyframe(seq, 0, 24, 1.0f);
    setOrAddKeyframe(seq, 0, 30, 0.0f);
    setOrAddKeyframe(seq, 0, 104, 1.0f);
    setOrAddKeyframe(seq, 0, 110, 0.0f);
    setOrAddKeyframe(seq, 0, 176, 1.0f);
    setOrAddKeyframe(seq, 0, 182, 0.0f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 0.0f);
    seq.Fit();
}

}  // namespace

// Register animatable fields once. This specialization is shared by the
// Inspector UI, Tanim, and ofxEnTTStateCollector.
template<>
inline void inspector::registerProperties(DemoAnimComponent& c, inspector::ComponentInspector& ci) {
    ci.addProperty("position", &c.position, -1000.f, 1000.f);
    ci.addProperty("rotation", &c.rotation, -360.f,  360.f);
    ci.addProperty("toggle",   &c.toggle);
    ci.addProperty("bangPulse",&c.bangPulse);
}

namespace tanim {

entt::entity FindEntityOfUID(const ComponentData& cdata, const std::string& uidToFind) {
    if (!cdata.m_user_data.has_value()) {
        LogError("FindEntityOfUID: m_user_data is empty for uid " + uidToFind);
        return entt::null;
    }

    const auto* userData = std::any_cast<TanimUserData>(&cdata.m_user_data);
    if (!userData || userData->registry == nullptr) {
        LogError("FindEntityOfUID: invalid user data for uid " + uidToFind);
        return entt::null;
    }

    const auto it = userData->uidToEntity.find(uidToFind);
    if (it == userData->uidToEntity.end()) {
        LogError("FindEntityOfUID: uid not found " + uidToFind);
        return entt::null;
    }

    return userData->registry->valid(it->second) ? it->second : entt::null;
}

void LogError(const std::string& message) { ofLogError("Tanim") << message; }

void LogInfo(const std::string& message) { ofLogNotice("Tanim") << message; }

}  // namespace tanim

void SurfingTimelineManager::addParameters(ofParameterGroup& parameters) {
    parametersGroup_ = &parameters;

    vec3Bindings_.clear();
    boolBindings_.clear();
    bangBindings_.clear();
    trackSpecs_.clear();

    scanGroup(parameters);
}

void SurfingTimelineManager::setBoolToggleCallback(BoolToggleCallback callback) { boolToggleCallback_ = std::move(callback); }

void SurfingTimelineManager::setBangCallback(BangCallback callback) { bangCallback_ = std::move(callback); }

void SurfingTimelineManager::setup(const std::string& imguiIniPath) {
    imguiIniPath_ = imguiIniPath;

    tanim::Init();
    tanim::RegisterComponent<DemoAnimComponent>("DemoAnimComponent");

    if (!imguiIniPath_.empty()) {
        ImGui::GetIO().IniFilename = imguiIniPath_.c_str();
        ofFile iniFile(imguiIniPath_);
        if (iniFile.exists()) {
            ImGui::LoadIniSettingsFromDisk(imguiIniPath_.c_str());
            pendingLateIniReload_ = true;
        }
    }

    setupScene();
    setupTimeline();

    tanim::OpenForEditing(registry_, entityDatas_, timelineData_, componentData_);
    tanim::EnterPlayMode();
    tanim::StartTimeline(timelineData_, componentData_);
    isPlayMode_ = true;

    syncParametersFromComponent();
    setupDone_ = true;
}

void SurfingTimelineManager::update(float deltaTime) {
    if (!setupDone_) {
        return;
    }

    tanim::UpdateEditor(deltaTime);
    if (isPlayMode_) {
        tanim::UpdateTimeline(registry_, entityDatas_, timelineData_, componentData_, deltaTime);
    }

    syncParametersFromComponent();
}

void SurfingTimelineManager::drawControlsAndTimeline() {
    if (!setupDone_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timeline Controls");

    ImGui::Text("Auto-bridge ofParameter <-> Timeline");
    ImGui::Separator();
    ImGui::Text("Tracks: %d", static_cast<int>(timelineData_.m_sequences.size()));
    ImGui::Text("Mode: %s", isPlayMode_ ? "PLAY" : "EDIT");
    ImGui::Text("Playing: %s", tanim::IsPlaying(componentData_) ? "YES" : "NO");

    if (ImGui::Button("Open Timeline Editor")) {
        tanim::OpenForEditing(registry_, entityDatas_, timelineData_, componentData_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        tanim::ResetEditorLayout();
    }

    if (!isPlayMode_) {
        if (ImGui::Button("Enter Play Mode")) {
            tanim::EnterPlayMode();
            tanim::StartTimeline(timelineData_, componentData_);
            isPlayMode_ = true;
        }
    } else {
        if (ImGui::Button("Exit Play Mode")) {
            tanim::StopTimeline(componentData_);
            tanim::ExitPlayMode();
            isPlayMode_ = false;
        }
    }

    if (!isPlayMode_) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Play")) {
        tanim::Play(componentData_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        tanim::Pause(componentData_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        tanim::Stop(componentData_);
    }

    if (!isPlayMode_) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::Text("Detected Parameters");

    for (auto& binding : vec3Bindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter);
        }
    }
    for (auto& binding : boolBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter);
        }
    }
    for (auto& binding : bangBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter, 120.0f);
        }
    }

    ImGui::End();

    if (pendingLateIniReload_ && !imguiIniPath_.empty()) {
        ImGui::LoadIniSettingsFromDisk(imguiIniPath_.c_str());
        pendingLateIniReload_ = false;
    }

    tanim::Draw();
}

void SurfingTimelineManager::exit() {
    if (!imguiIniPath_.empty()) {
        ImGui::SaveIniSettingsToDisk(imguiIniPath_.c_str());
    }

    tanim::CloseEditor();
    if (isPlayMode_) {
        tanim::StopTimeline(componentData_);
        tanim::ExitPlayMode();
        isPlayMode_ = false;
    }

    setupDone_ = false;
}

bool SurfingTimelineManager::isReady() const { return setupDone_ && hasSceneEntity(); }

glm::vec3 SurfingTimelineManager::getPosition() const {
    glm::vec3 value(0.0f);
    readVec3Field("position", value);
    return value;
}

glm::vec3 SurfingTimelineManager::getRotationDeg() const {
    glm::vec3 value(0.0f);
    readVec3Field("rotation", value);
    return value;
}

bool SurfingTimelineManager::isToggleEnabled() const {
    bool value = true;
    readBoolField("toggle", value);
    return value;
}

void SurfingTimelineManager::scanGroup(ofParameterGroup& group) {
    for (std::size_t i = 0; i < group.size(); ++i) {
        auto& parameter = group.get(i);

        if (group.getType(i) == typeid(ofParameterGroup).name()) {
            scanGroup(parameter.castGroup());
            continue;
        }

        if (parameter.isReadOnly()) {
            continue;
        }

        if (parameter.isOfType<glm::vec3>()) {
            addVec3Parameter(parameter.cast<glm::vec3>());
            continue;
        }

        if (parameter.isOfType<bool>()) {
            addBoolParameter(parameter.cast<bool>());
            continue;
        }

        if (parameter.isOfType<void>()) {
            addBangParameter(parameter.cast<void>());
            continue;
        }
    }
}

void SurfingTimelineManager::addVec3Parameter(ofParameter<glm::vec3>& parameter) {
    const std::string fieldName = resolveVec3Field(parameter.getName());
    if (fieldName.empty()) {
        ofLogNotice("SurfingTimelineManager") << "Ignoring vec3 parameter '" << parameter.getName() << "'";
        return;
    }

    auto it = std::find_if(vec3Bindings_.begin(),
                           vec3Bindings_.end(),
                           [&fieldName](const Vec3Binding& binding) { return binding.fieldName == fieldName; });

    if (it == vec3Bindings_.end()) {
        vec3Bindings_.push_back({fieldName, parameter.getName(), &parameter, nullptr});
        it = std::prev(vec3Bindings_.end());
    } else {
        it->parameterName = parameter.getName();
        it->parameter = &parameter;
    }

    it->listener = parameter.newListener([this, fieldName](glm::vec3& value) { onVec3ParameterChanged(fieldName, value); });
    registerTrack(fieldName, fieldName == "rotation" ? TrackKind::Rotation : TrackKind::Position);
}

void SurfingTimelineManager::addBoolParameter(ofParameter<bool>& parameter) {
    const std::string fieldName = resolveBoolField(parameter.getName());
    if (fieldName.empty()) {
        ofLogNotice("SurfingTimelineManager") << "Ignoring bool parameter '" << parameter.getName() << "'";
        return;
    }

    auto it = std::find_if(boolBindings_.begin(),
                           boolBindings_.end(),
                           [&fieldName](const BoolBinding& binding) { return binding.fieldName == fieldName; });

    if (it == boolBindings_.end()) {
        boolBindings_.push_back({fieldName, parameter.getName(), &parameter, nullptr});
        it = std::prev(boolBindings_.end());
    } else {
        it->parameterName = parameter.getName();
        it->parameter = &parameter;
    }

    const std::string parameterName = parameter.getName();
    it->listener = parameter.newListener([this, fieldName, parameterName](bool& value) {
        onBoolParameterChanged(fieldName, parameterName, value);
    });
    registerTrack(fieldName, TrackKind::Toggle);
}

void SurfingTimelineManager::addBangParameter(ofParameter<void>& parameter) {
    const std::string fieldName = resolveBangField(parameter.getName());
    if (fieldName.empty()) {
        ofLogNotice("SurfingTimelineManager") << "Ignoring bang parameter '" << parameter.getName() << "'";
        return;
    }

    auto it = std::find_if(bangBindings_.begin(),
                           bangBindings_.end(),
                           [&fieldName](const BangBinding& binding) { return binding.fieldName == fieldName; });

    if (it == bangBindings_.end()) {
        bangBindings_.push_back({fieldName, parameter.getName(), &parameter, nullptr, false});
        it = std::prev(bangBindings_.end());
    } else {
        it->parameterName = parameter.getName();
        it->parameter = &parameter;
    }

    const std::string parameterName = parameter.getName();
    it->listener = parameter.newListener([this, parameterName]() { onBangParameterTriggered(parameterName); });
    registerTrack(fieldName, TrackKind::BangPulse);
}

std::string SurfingTimelineManager::normalizeName(const std::string& name) const {
    std::string result;
    result.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            result.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return result;
}

std::string SurfingTimelineManager::resolveVec3Field(const std::string& parameterName) const {
    const std::string name = normalizeName(parameterName);

    if (name.find("pos") != std::string::npos || name.find("position") != std::string::npos ||
        name.find("translate") != std::string::npos) {
        return "position";
    }

    if (name.find("rot") != std::string::npos || name.find("rotation") != std::string::npos ||
        name.find("euler") != std::string::npos) {
        return "rotation";
    }

    if (!hasVec3Field("position")) {
        return "position";
    }

    if (!hasVec3Field("rotation")) {
        return "rotation";
    }

    return "";
}

std::string SurfingTimelineManager::resolveBoolField(const std::string& parameterName) const {
    const std::string name = normalizeName(parameterName);

    if (name.find("toggle") != std::string::npos || name.find("visible") != std::string::npos ||
        name.find("enable") != std::string::npos || name.find("show") != std::string::npos) {
        return "toggle";
    }

    if (!hasBoolField("toggle")) {
        return "toggle";
    }

    return "";
}

std::string SurfingTimelineManager::resolveBangField(const std::string& parameterName) const {
    const std::string name = normalizeName(parameterName);

    if (name.find("bang") != std::string::npos || name.find("trigger") != std::string::npos ||
        name.find("pulse") != std::string::npos) {
        return "bangPulse";
    }

    if (!hasBangField("bangPulse")) {
        return "bangPulse";
    }

    return "";
}

bool SurfingTimelineManager::hasVec3Field(const std::string& fieldName) const {
    return std::any_of(vec3Bindings_.begin(),
                       vec3Bindings_.end(),
                       [&fieldName](const Vec3Binding& binding) { return binding.fieldName == fieldName; });
}

bool SurfingTimelineManager::hasBoolField(const std::string& fieldName) const {
    return std::any_of(boolBindings_.begin(),
                       boolBindings_.end(),
                       [&fieldName](const BoolBinding& binding) { return binding.fieldName == fieldName; });
}

bool SurfingTimelineManager::hasBangField(const std::string& fieldName) const {
    return std::any_of(bangBindings_.begin(),
                       bangBindings_.end(),
                       [&fieldName](const BangBinding& binding) { return binding.fieldName == fieldName; });
}

void SurfingTimelineManager::registerTrack(const std::string& fieldName, TrackKind kind) {
    auto it = std::find_if(trackSpecs_.begin(),
                           trackSpecs_.end(),
                           [&fieldName](const TrackSpec& track) { return track.fieldName == fieldName; });

    if (it == trackSpecs_.end()) {
        trackSpecs_.push_back({fieldName, kind});
        return;
    }

    it->kind = kind;
}

void SurfingTimelineManager::setupScene() {
    registry_.clear();
    entityDatas_.clear();

    animatedEntity_ = registry_.create();
    registry_.emplace<DemoNameComponent>(animatedEntity_, "entity.cube", "Animated Cube");

    DemoAnimComponent component;

    for (const auto& binding : vec3Bindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }
        if (binding.fieldName == "position") {
            component.position = binding.parameter->get();
        } else if (binding.fieldName == "rotation") {
            component.rotation = binding.parameter->get();
        }
    }

    for (const auto& binding : boolBindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }
        if (binding.fieldName == "toggle") {
            component.toggle = binding.parameter->get();
        }
    }

    registry_.emplace<DemoAnimComponent>(animatedEntity_, component);

    const auto& nameComponent = registry_.get<DemoNameComponent>(animatedEntity_);
    entityDatas_ = {{nameComponent.uid, nameComponent.display}};

    TanimUserData userData;
    userData.registry = &registry_;
    userData.uidToEntity.emplace(nameComponent.uid, animatedEntity_);
    componentData_.m_user_data = std::move(userData);
}

void SurfingTimelineManager::setupTimeline() {
    if (trackSpecs_.empty()) {
        registerTrack("position", TrackKind::Position);
        registerTrack("rotation", TrackKind::Rotation);
    }

    timelineData_.m_name = "Surfing Vec3 Cube Timeline";
    timelineData_.m_last_frame = TIMELINE_LAST_FRAME;
    timelineData_.m_max_frame = TIMELINE_MAX_FRAME;
    timelineData_.m_player_samples = 60;
    timelineData_.m_playback_type = tanim::internal::PlaybackType::LOOP;
    timelineData_.m_play_immediately = true;
    timelineData_.m_sequences.clear();

    for (const auto& track : trackSpecs_) {
        auto* seq = addSequenceByField(registry_, entityDatas_.front(), timelineData_, componentData_, track.fieldName);
        if (seq == nullptr) {
            ofLogWarning("SurfingTimelineManager") << "Missing field for sequence: " << track.fieldName;
            continue;
        }

        switch (track.kind) {
            case TrackKind::Position:
                configurePositionSequence(*seq);
                break;
            case TrackKind::Rotation:
                configureRotationSequence(*seq);
                break;
            case TrackKind::Toggle:
                configureToggleSequence(*seq);
                break;
            case TrackKind::BangPulse:
                configureBangPulseSequence(*seq);
                break;
            default:
                break;
        }
    }
}

void SurfingTimelineManager::syncParametersFromComponent() {
    if (!hasSceneEntity()) {
        return;
    }

    isSyncingFromAnimation_ = true;

    for (auto& binding : vec3Bindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }

        glm::vec3 value(0.0f);
        if (!readVec3Field(binding.fieldName, value)) {
            continue;
        }

        if (glm::distance(binding.parameter->get(), value) > 0.0001f) {
            binding.parameter->set(value);
        }
    }

    for (auto& binding : boolBindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }

        bool value = false;
        if (!readBoolField(binding.fieldName, value)) {
            continue;
        }

        if (binding.parameter->get() != value) {
            binding.parameter->set(value);
        }
    }

    isSyncingFromAnimation_ = false;

    for (auto& binding : bangBindings_) {
        bool pulse = false;
        if (!readBoolField(binding.fieldName, pulse)) {
            continue;
        }

        if (pulse && !binding.previousPulse) {
            if (binding.parameter != nullptr) {
                binding.parameter->trigger();
            } else if (bangCallback_) {
                bangCallback_(binding.parameterName);
            }
        }

        binding.previousPulse = pulse;
    }
}

void SurfingTimelineManager::onVec3ParameterChanged(const std::string& fieldName, const glm::vec3& value) {
    if (isSyncingFromAnimation_) {
        return;
    }

    writeVec3Field(fieldName, value);
}

void SurfingTimelineManager::onBoolParameterChanged(const std::string& fieldName,
                                                    const std::string& parameterName,
                                                    bool value) {
    if (boolToggleCallback_) {
        boolToggleCallback_(parameterName, value);
    }

    if (isSyncingFromAnimation_) {
        return;
    }

    writeBoolField(fieldName, value);
}

void SurfingTimelineManager::onBangParameterTriggered(const std::string& parameterName) {
    if (bangCallback_) {
        bangCallback_(parameterName);
    }
}

bool SurfingTimelineManager::readVec3Field(const std::string& fieldName, glm::vec3& value) const {
    if (!hasSceneEntity()) {
        return false;
    }

    const auto& component = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "position") {
        value = component.position;
        return true;
    }

    if (fieldName == "rotation") {
        value = component.rotation;
        return true;
    }

    return false;
}

bool SurfingTimelineManager::readBoolField(const std::string& fieldName, bool& value) const {
    if (!hasSceneEntity()) {
        return false;
    }

    const auto& component = registry_.get<DemoAnimComponent>(animatedEntity_);

    if (fieldName == "toggle") {
        value = component.toggle;
        return true;
    }

    if (fieldName == "bangPulse") {
        value = component.bangPulse;
        return true;
    }

    return false;
}

void SurfingTimelineManager::writeVec3Field(const std::string& fieldName, const glm::vec3& value) {
    if (!hasSceneEntity()) {
        return;
    }

    auto& component = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "position") {
        component.position = value;
    } else if (fieldName == "rotation") {
        component.rotation = value;
    }
}

void SurfingTimelineManager::writeBoolField(const std::string& fieldName, bool value) {
    if (!hasSceneEntity()) {
        return;
    }

    auto& component = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "toggle") {
        component.toggle = value;
    } else if (fieldName == "bangPulse") {
        component.bangPulse = value;
    }
}

bool SurfingTimelineManager::hasSceneEntity() const {
    return registry_.valid(animatedEntity_) && registry_.all_of<DemoAnimComponent>(animatedEntity_);
}

// ─────────────────────────────────────────────────────────────────────────────
// ofApp
// ─────────────────────────────────────────────────────────────────────────────

float ofApp::computeUiScale() const {
    // Use DisplayFramebufferScale set by the GLFW backend (reliable on all
    // platforms including HiDPI Windows and Retina macOS).
    // Falls back to 1.0 before the first ImGui frame.
    const float s = ImGui::GetIO().DisplayFramebufferScale.x;
    return (s > 0.1f) ? s : 1.0f;
}

void ofApp::setupImGui() {
    // Docking only: popped-out Dear ImGui OS windows render in GLFW contexts where the
    // viewport's FBO texture may not bind correctly; see ImGui_ImplGlfw_CreateWindow + share contexts.
    gui_.setup(nullptr, true, ImGuiConfigFlags_DockingEnable);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Load the default font at 15 px — crisp at 1080p and readable at 4K.
    // On HiDPI displays the GLFW backend scales the framebuffer automatically;
    // we intentionally avoid ScaleAllSizes() to prevent double-scaling.
    ImFontConfig cfg;
    cfg.SizePixels = 15.0f;
    io.Fonts->Clear();
    io.Fonts->AddFontDefault(&cfg);
    io.Fonts->Build();

    // Easier to grab dock splitters; complements clearing NoResize flags on nodes (see beginDockSpace).
    ImGuiStyle& style = ImGui::GetStyle();
    style.DockingSeparatorSize = std::max(style.DockingSeparatorSize, 5.0f);
}

void ofApp::resizeViewport(int w, int h) {
    if (w < 2 || h < 2) return;
    viewportSize_ = glm::vec2(static_cast<float>(w), static_cast<float>(h));
    ofFbo::Settings s;
    s.width          = w;
    s.height         = h;
    s.useDepth       = true;
    s.internalformat = GL_RGBA;
    s.textureTarget  = GL_TEXTURE_2D;
    s.numSamples     = 0;
    viewport_.allocate(s);
    camera_.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
}

void ofApp::setup() {
    ofSetLogLevel(OF_LOG_NOTICE);
    ofSetVerticalSync(true);
    ofSetFrameRate(60);

    // ofFbo and ImGui both need normalised (0-1) UVs, which requires
    // GL_TEXTURE_2D.  Disable the OpenGL-extension ARB rectangle textures
    // globally so every subsequent ofFbo/ofTexture allocation uses 2D.
    ofDisableArbTex();

    setupImGui();

    camera_.setDistance(550.0f);
    camera_.setNearClip(0.1f);
    camera_.setFarClip(6000.0f);

    resizeViewport(ofGetWidth(), ofGetHeight());

    setupParameters();
    timelineManager_.addParameters(parameters_);
    timelineManager_.setBoolToggleCallback(
        [this](const std::string& n, bool v) { onBoolToggle(n, v); });
    timelineManager_.setBangCallback(
        [this](const std::string& n) { onBang(n); });
    timelineManager_.setup(ofToDataPath("imgui.ini", true));
}

void ofApp::update() {
    timelineManager_.update(ofGetLastFrameTime());
    bangFlash_ = std::max(0.0f, bangFlash_ - static_cast<float>(ofGetLastFrameTime()) * 2.8f);
}

void ofApp::draw() {
    ofBackground(ofColor(15, 18, 26));

    gui_.begin();
    beginDockSpace();
    drawViewportWindow();
    timelineManager_.drawControlsAndTimeline();
    drawCallbacksWindow();
    gui_.end();
}

void ofApp::exit() { timelineManager_.exit(); }

void ofApp::windowResized(int w, int h) {
    (void)w;
    (void)h;
    // Do not clamp ImGui-measured dock sizes against pixel `w`,`h`: they mix logical vs framebuffer
    // units and corrupt layout. Dock + FBO extents are rebuilt from `GetContentRegionAvail()` each frame.
}

// ─── DockSpace ───────────────────────────────────────────────────────────────
void ofApp::beginDockSpace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking             |
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus            |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##DockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockId = ImGui::GetID("MainDockSpace");
    // PassthruCentralNode can steal/disable splitter hit-testing on some drivers / layouts — keep default.
    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f));

    // Older / hand-edited imgui.ini files can persist ImGuiDockNodeFlags_NoResize on splits; that makes
    // the sidebar (and other dock splits) impossible to drag. Clear those locks on the live tree each frame.
    if (ImGuiDockNode* dockRoot = ImGui::DockBuilderGetNode(dockId))
    {
        dockRoot->SharedFlags &= ~(ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoResizeX |
                                   ImGuiDockNodeFlags_NoResizeY);
        clearImGuiDockNodeResizeLocks(dockRoot);
    }

    // Check for an explicit reset request (e.g. "Reset Layout" button).
    const bool forceRebuild = tanim::IsLayoutResetRequested();
    if (forceRebuild) {
        layoutBuilt_ = false;
        tanim::ClearLayoutResetRequest();
    }

    if (!layoutBuilt_) {
        // On a forced reset we always rebuild; otherwise skip if imgui.ini has a layout.
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockId);
        const bool hasSavedLayout = !forceRebuild && node && !node->IsLeafNode();
        if (!hasSavedLayout) {
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

            // Left sidebar (22%): controls + callbacks
            ImGuiID left, mainArea;
            ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.22f, &left, &mainArea);

            ImGuiID leftTop, leftBot;
            ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.75f, &leftTop, &leftBot);

            // Main area: viewport (top 58%) | editor strip (bottom 42%)
            ImGuiID viewportNode, editorStrip;
            ImGui::DockBuilderSplitNode(mainArea, ImGuiDir_Up, 0.58f, &viewportNode, &editorStrip);

            ImGui::DockBuilderDockWindow("Viewport",          viewportNode);
            ImGui::DockBuilderDockWindow("Timeline Controls", leftTop);
            ImGui::DockBuilderDockWindow("Callbacks",         leftBot);

            // Tanim fills the editor strip — must be called BEFORE DockBuilderFinish.
            tanim::BuildEditorLayout(editorStrip);

            ImGui::DockBuilderFinish(dockId);
        }
        layoutBuilt_ = true;
    }

    ImGui::End();
}

// ─── 3D Viewport ─────────────────────────────────────────────────────────────
void ofApp::renderSceneToViewport() {
    // Dear ImGui may leave GL_SCISSOR_TEST enabled from a previous frame's draw;
    // scissor coordinates are for the default framebuffer and can clip all FBO draws.
    const GLboolean scissorsOn = glIsEnabled(GL_SCISSOR_TEST);
    if (scissorsOn) {
        glDisable(GL_SCISSOR_TEST);
    }

    viewport_.begin();

    // ofBackgroundGradient draws without writing depth, so the depth attachment
    // stays undefined unless cleared — depth-tested 3D can disappear entirely.
    ofClear(20, 24, 32);
    ofBackgroundGradient(ofColor(20, 24, 32), ofColor(8, 10, 14), OF_GRADIENT_LINEAR);
    drawScene3d();

    viewport_.end();

    if (scissorsOn) {
        glEnable(GL_SCISSOR_TEST);
    }
}

void ofApp::drawViewportWindow() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin(
        "Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (visible) {
        ImGuiIO& io          = ImGui::GetIO();
        viewportFbScale_.x = (io.DisplayFramebufferScale.x > 0.001f) ? io.DisplayFramebufferScale.x : 1.0f;
        viewportFbScale_.y = (io.DisplayFramebufferScale.y > 0.001f) ? io.DisplayFramebufferScale.y : 1.0f;

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 4.0f && avail.y > 4.0f) {
            // Same-frame sizing: avail is ImGui logical units × framebuffer scale = pixel-ish FBO size.
            const int rw =
                std::clamp(static_cast<int>(std::lround(static_cast<float>(avail.x * viewportFbScale_.x))),
                           2, std::max(2, ofGetWidth()));
            const int rh =
                std::clamp(static_cast<int>(std::lround(static_cast<float>(avail.y * viewportFbScale_.y))),
                           2, std::max(2, ofGetHeight()));
            if (rw != static_cast<int>(viewport_.getWidth()) || rh != static_cast<int>(viewport_.getHeight())) {
                resizeViewport(rw, rh);
            }
            // Render after docking layout knows the panel size (avoids stale 1‑frame allocations).
            renderSceneToViewport();

            const ofTexture& vt = viewport_.getTexture();
            if (vt.getTextureData().textureTarget == GL_TEXTURE_2D && vt.texData.textureID != 0) {
                ImGui::Image(GetImTextureID(vt), avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            } else {
                ImGui::TextUnformatted("Viewport FBO texture is unavailable (needs GL_TEXTURE_2D).");
            }
        }
    }

    ImGui::End();
}

// ─── Callbacks panel ─────────────────────────────────────────────────────────
void ofApp::drawCallbacksWindow() {
    ImGui::SetNextWindowSize(ImVec2(360.0f, 120.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Callbacks");
    ImGui::Text("Last bool:  %s", lastBoolEvent_.c_str());
    ImGui::Text("Last bang:  %s", lastBangEvent_.c_str());
    ImGui::Text("Bang count: %llu", static_cast<unsigned long long>(bangCounter_));
    ImGui::End();
}

// ─── Scene ───────────────────────────────────────────────────────────────────
void ofApp::setupParameters() {
    parameters_.setName("Cube Parameters");
    parameters_.add(pPosition_.set("Position",
        {0.0f, 0.0f, 0.0f}, {-300.0f, -240.0f, -300.0f}, {300.0f, 240.0f, 300.0f}));
    parameters_.add(pRotationDeg_.set("Rotation",
        {0.0f, 0.0f, 0.0f}, {-360.0f, -720.0f, -360.0f}, {360.0f, 720.0f, 360.0f}));
    parameters_.add(pToggle_.set("Visible", true));
    parameters_.add(pBang_.set("Bang"));
}

void ofApp::drawScene3d() {
    const glm::vec3 pos    = timelineManager_.getPosition();
    const glm::vec3 rotDeg = timelineManager_.getRotationDeg();
    const bool      vis    = timelineManager_.isToggleEnabled();

    const ofRectangle vp(0.0f, 0.0f, viewportSize_.x, viewportSize_.y);
    ofEnableDepthTest();
    camera_.begin(vp);

    // Grid
    ofPushStyle();
    ofSetColor(68, 72, 84);
    const float gridSize = 800.0f;
    for (int i = -4; i <= 4; ++i) {
        const float p = static_cast<float>(i) * 100.0f;
        ofDrawLine(glm::vec3(-gridSize, 0.0f, p), glm::vec3(gridSize, 0.0f, p));
        ofDrawLine(glm::vec3(p, 0.0f, -gridSize), glm::vec3(p, 0.0f, gridSize));
    }
    ofPopStyle();

    // Cube
    ofPushMatrix();
    ofTranslate(pos.x, pos.y, pos.z);
    ofRotateXDeg(rotDeg.x);
    ofRotateYDeg(rotDeg.y);
    ofRotateZDeg(rotDeg.z);

    const ofColor fill   = vis ? ofColor(70, 215, 170) : ofColor(90, 96, 108);
    const ofColor stroke = vis ? ofColor(225, 250, 240) : ofColor(140, 146, 156);

    if (vis) {
        ofSetColor(fill);
        ofDrawBox(0.0f, 0.0f, 0.0f, 140.0f);
    }
    ofNoFill();
    ofSetLineWidth(2.0f);
    ofSetColor(stroke);
    ofDrawBox(0.0f, 0.0f, 0.0f, 140.0f);
    ofFill();

    if (bangFlash_ > 0.0f) {
        const float alpha = ofMap(bangFlash_, 0.0f, 1.0f, 0.0f, 180.0f, true);
        ofSetColor(255, 220, 90, static_cast<int>(alpha));
        ofDrawSphere(0.0f, 0.0f, 0.0f, 120.0f + bangFlash_ * 60.0f);
    }

    ofPopMatrix();
    camera_.end();
    ofDisableDepthTest();
}

// ─── Callbacks ───────────────────────────────────────────────────────────────
void ofApp::onBoolToggle(const std::string& name, bool value) {
    lastBoolEvent_ = name + " -> " + (value ? "true" : "false");
    ofLogNotice("ofApp") << "Bool: " << lastBoolEvent_;
}

void ofApp::onBang(const std::string& name) {
    ++bangCounter_;
    bangFlash_     = 1.0f;
    lastBangEvent_ = name + " #" + ofToString(bangCounter_);
    ofLogNotice("ofApp") << "Bang: " << lastBangEvent_;
}
