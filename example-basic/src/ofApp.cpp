#include "ofApp.h"

#include <algorithm>
#include <any>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

#include "ImHelpers.h"
#include "tanim/registry.hpp"
#include "tanim/sequence_id.hpp"
#include "tanim/tanim_user.hpp"
#include "ComponentInspector.h"

#include <imgui.h>

namespace {

struct DemoAnimComponent {
    bool toggle{false};
    float value01{0.15f};
    int index08{0};
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
constexpr float BANG_FLASH_DURATION = 0.25f;

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

void configureToggleSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 0.0f);
    setOrAddKeyframe(seq, 0, 36, 1.0f);
    setOrAddKeyframe(seq, 0, 84, 0.0f);
    setOrAddKeyframe(seq, 0, 132, 1.0f);
    setOrAddKeyframe(seq, 0, 180, 0.0f);
    setOrAddKeyframe(seq, 0, 220, 1.0f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 0.0f);
    seq.Fit();
}

void configureFloatSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 0.10f);
    setOrAddKeyframe(seq, 0, 50, 0.88f);
    setOrAddKeyframe(seq, 0, 100, 0.35f);
    setOrAddKeyframe(seq, 0, 150, 0.95f);
    setOrAddKeyframe(seq, 0, 190, 0.20f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 0.70f);
    seq.Fit();
}

void configureIntSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 0.0f);
    setOrAddKeyframe(seq, 0, 30, 1.0f);
    setOrAddKeyframe(seq, 0, 70, 3.0f);
    setOrAddKeyframe(seq, 0, 110, 5.0f);
    setOrAddKeyframe(seq, 0, 150, 8.0f);
    setOrAddKeyframe(seq, 0, 190, 4.0f);
    setOrAddKeyframe(seq, 0, 220, 2.0f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 0.0f);
    seq.Fit();
}

void configureBangPulseSequence(tanim::internal::Sequence& seq) {
    seq.EditLastFrame(TIMELINE_LAST_FRAME);
    setOrAddKeyframe(seq, 0, 0, 0.0f);
    setOrAddKeyframe(seq, 0, 24, 1.0f);
    setOrAddKeyframe(seq, 0, 28, 0.0f);
    setOrAddKeyframe(seq, 0, 96, 1.0f);
    setOrAddKeyframe(seq, 0, 100, 0.0f);
    setOrAddKeyframe(seq, 0, 168, 1.0f);
    setOrAddKeyframe(seq, 0, 172, 0.0f);
    setOrAddKeyframe(seq, 0, 216, 1.0f);
    setOrAddKeyframe(seq, 0, 220, 0.0f);
    setOrAddKeyframe(seq, 0, TIMELINE_LAST_FRAME, 0.0f);
    seq.Fit();
}

}  // namespace

// Register animatable fields once. This specialization is shared by the
// Inspector UI, Tanim, and ofxEnTTStateCollector.
template<>
inline void inspector::registerProperties(DemoAnimComponent& c, inspector::ComponentInspector& ci) {
    ci.addProperty("toggle",    &c.toggle);
    ci.addProperty("value01",   &c.value01, 0.f, 1.f);
    ci.addProperty("index08",   &c.index08, 0,   8);
    ci.addProperty("bangPulse", &c.bangPulse);
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

void TimelineDemoManager::addParameter(const std::string& fieldName, ofParameter<bool>& parameter) {
    auto it = std::find_if(boolBindings_.begin(),
                           boolBindings_.end(),
                           [&fieldName](const BoolBinding& binding) { return binding.fieldName == fieldName; });

    if (it == boolBindings_.end()) {
        boolBindings_.emplace_back();
        it = std::prev(boolBindings_.end());
        it->fieldName = fieldName;
        it->parameter = &parameter;
    } else {
        it->parameter = &parameter;
    }

    it->listener = parameter.newListener([this, fieldName](bool& value) { onBoolParameterChanged(fieldName, value); });
    registerTrack(fieldName, fieldName == "bangPulse" ? TrackKind::BangPulse : TrackKind::Toggle);
}

void TimelineDemoManager::addParameter(const std::string& fieldName, ofParameter<float>& parameter) {
    auto it = std::find_if(floatBindings_.begin(),
                           floatBindings_.end(),
                           [&fieldName](const FloatBinding& binding) { return binding.fieldName == fieldName; });

    if (it == floatBindings_.end()) {
        floatBindings_.emplace_back();
        it = std::prev(floatBindings_.end());
        it->fieldName = fieldName;
        it->parameter = &parameter;
    } else {
        it->parameter = &parameter;
    }

    it->listener = parameter.newListener([this, fieldName](float& value) { onFloatParameterChanged(fieldName, value); });
    registerTrack(fieldName, TrackKind::Float01);
}

void TimelineDemoManager::addParameter(const std::string& fieldName, ofParameter<int>& parameter) {
    auto it = std::find_if(intBindings_.begin(),
                           intBindings_.end(),
                           [&fieldName](const IntBinding& binding) { return binding.fieldName == fieldName; });

    if (it == intBindings_.end()) {
        intBindings_.emplace_back();
        it = std::prev(intBindings_.end());
        it->fieldName = fieldName;
        it->parameter = &parameter;
    } else {
        it->parameter = &parameter;
    }

    it->listener = parameter.newListener([this, fieldName](int& value) { onIntParameterChanged(fieldName, value); });
    registerTrack(fieldName, TrackKind::Int08);
}

void TimelineDemoManager::addBangParameter(const std::string& fieldName, ofParameter<void>& parameter) {
    auto it = std::find_if(bangBindings_.begin(),
                           bangBindings_.end(),
                           [&fieldName](const BangBinding& binding) { return binding.fieldName == fieldName; });

    if (it == bangBindings_.end()) {
        bangBindings_.emplace_back();
        it = std::prev(bangBindings_.end());
        it->fieldName = fieldName;
        it->parameter = &parameter;
    } else {
        it->parameter = &parameter;
    }

    it->listener = parameter.newListener([this]() { onBangParameterTriggered(); });
    registerTrack(fieldName, TrackKind::BangPulse);
}

void TimelineDemoManager::setup(const std::string& imguiIniPath) {
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

void TimelineDemoManager::update(float deltaTime) {
    if (!setupDone_) {
        return;
    }

    tanim::UpdateEditor(deltaTime);

    if (isPlayMode_) {
        tanim::UpdateTimeline(registry_, entityDatas_, timelineData_, componentData_, deltaTime);
    }

    syncParametersFromComponent();
    bangFlashTimer_ = std::max(0.0f, bangFlashTimer_ - deltaTime);
}

void TimelineDemoManager::drawControlsAndTimeline() {
    if (!setupDone_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Example Controls");
    ImGui::Text("Timeline + ofParameter bridge");
    ImGui::Separator();

    const std::string entityDisplay = entityDatas_.empty() ? "-" : entityDatas_.front().m_display;
    ImGui::Text("Entity: %s", entityDisplay.c_str());
    ImGui::Text("Tracks: %d", static_cast<int>(timelineData_.m_sequences.size()));

    if (ImGui::Button("Open Timeline Editor")) {
        tanim::OpenForEditing(registry_, entityDatas_, timelineData_, componentData_);
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
    for (auto& binding : boolBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter);
        }
    }

    for (auto& binding : floatBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter);
        }
    }

    for (auto& binding : intBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter);
        }
    }

    for (auto& binding : bangBindings_) {
        if (binding.parameter != nullptr) {
            ofxImGui::AddParameter(*binding.parameter, 120.0f);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Reset UI layout")) {
        tanim::ResetEditorLayout();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Restore default docking (Example Controls left, Demo Scene center, timeline editor below)");
    }

    ImGui::Separator();
    ImGui::Text("Mode: %s", isPlayMode_ ? "PLAY" : "EDIT");
    ImGui::Text("Playing: %s", tanim::IsPlaying(componentData_) ? "YES" : "NO");
    ImGui::Text("Bang count: %llu", static_cast<unsigned long long>(bangCounter_));

    ImGui::End();

    tanim::Draw();

    if (pendingLateIniReload_ && !imguiIniPath_.empty()) {
        ImGui::LoadIniSettingsFromDisk(imguiIniPath_.c_str());
        pendingLateIniReload_ = false;
    }
}

void TimelineDemoManager::exit() {
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

bool TimelineDemoManager::isReady() const { return setupDone_ && hasSceneEntity(); }

bool TimelineDemoManager::isToggleOn() const {
    bool value = false;
    readBoolField("toggle", value);
    return value;
}

float TimelineDemoManager::getValue01() const {
    float value = 0.0f;
    readFloatField("value01", value);
    return value;
}

int TimelineDemoManager::getIndex08() const {
    int value = 0;
    readIntField("index08", value);
    return value;
}

float TimelineDemoManager::getBangFlash01() const { return ofClamp(bangFlashTimer_ / BANG_FLASH_DURATION, 0.0f, 1.0f); }

std::uint64_t TimelineDemoManager::getBangCount() const { return bangCounter_; }

void TimelineDemoManager::registerTrack(const std::string& fieldName, TrackKind kind) {
    auto it = std::find_if(trackSpecs_.begin(),
                           trackSpecs_.end(),
                           [&fieldName](const TrackSpec& track) { return track.fieldName == fieldName; });

    if (it == trackSpecs_.end()) {
        trackSpecs_.push_back({fieldName, kind});
        return;
    }

    it->kind = kind;
}

void TimelineDemoManager::setupScene() {
    registry_.clear();
    entityDatas_.clear();

    animatedEntity_ = registry_.create();
    registry_.emplace<DemoNameComponent>(animatedEntity_, "entity.demo", "Timeline Parameter Demo");
    registry_.emplace<DemoAnimComponent>(animatedEntity_);

    const auto& nameComponent = registry_.get<DemoNameComponent>(animatedEntity_);
    entityDatas_ = {{nameComponent.uid, nameComponent.display}};

    TanimUserData userData;
    userData.registry = &registry_;
    userData.uidToEntity.emplace(nameComponent.uid, animatedEntity_);
    componentData_.m_user_data = std::move(userData);
}

void TimelineDemoManager::setupTimeline() {
    if (trackSpecs_.empty()) {
        registerTrack("toggle", TrackKind::Toggle);
        registerTrack("value01", TrackKind::Float01);
        registerTrack("index08", TrackKind::Int08);
        registerTrack("bangPulse", TrackKind::BangPulse);
    }

    timelineData_.m_name = "Intuitive Parameter Timeline";
    timelineData_.m_last_frame = TIMELINE_LAST_FRAME;
    timelineData_.m_max_frame = TIMELINE_MAX_FRAME;
    timelineData_.m_player_samples = 60;
    timelineData_.m_playback_type = tanim::internal::PlaybackType::LOOP;
    timelineData_.m_play_immediately = true;
    timelineData_.m_sequences.clear();

    for (const auto& track : trackSpecs_) {
        auto* seq = addSequenceByField(registry_, entityDatas_.front(), timelineData_, componentData_, track.fieldName);
        if (seq == nullptr) {
            ofLogWarning("TimelineDemoManager") << "Missing field for sequence: " << track.fieldName;
            continue;
        }

        switch (track.kind) {
            case TrackKind::Toggle:
                configureToggleSequence(*seq);
                break;
            case TrackKind::Float01:
                configureFloatSequence(*seq);
                break;
            case TrackKind::Int08:
                configureIntSequence(*seq);
                break;
            case TrackKind::BangPulse:
                configureBangPulseSequence(*seq);
                break;
            default:
                break;
        }
    }
}

void TimelineDemoManager::onBoolParameterChanged(const std::string& fieldName, bool value) {
    if (isSyncingFromAnimation_) {
        return;
    }

    writeBoolField(fieldName, value);
}

void TimelineDemoManager::onFloatParameterChanged(const std::string& fieldName, float value) {
    if (isSyncingFromAnimation_) {
        return;
    }

    writeFloatField(fieldName, value);
}

void TimelineDemoManager::onIntParameterChanged(const std::string& fieldName, int value) {
    if (isSyncingFromAnimation_) {
        return;
    }

    writeIntField(fieldName, value);
}

void TimelineDemoManager::onBangParameterTriggered() { triggerBangFeedback(); }

void TimelineDemoManager::syncParametersFromComponent() {
    if (!hasSceneEntity()) {
        return;
    }

    isSyncingFromAnimation_ = true;

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

    for (auto& binding : floatBindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }

        float value = 0.0f;
        if (!readFloatField(binding.fieldName, value)) {
            continue;
        }

        if (std::abs(binding.parameter->get() - value) > 0.0001f) {
            binding.parameter->set(value);
        }
    }

    for (auto& binding : intBindings_) {
        if (binding.parameter == nullptr) {
            continue;
        }

        int value = 0;
        if (!readIntField(binding.fieldName, value)) {
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
            } else {
                triggerBangFeedback();
            }
        }

        binding.previousPulse = pulse;
    }
}

void TimelineDemoManager::triggerBangFeedback() {
    bangFlashTimer_ = BANG_FLASH_DURATION;
    ++bangCounter_;
}

bool TimelineDemoManager::readBoolField(const std::string& fieldName, bool& value) const {
    if (!hasSceneEntity()) {
        return false;
    }

    const auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "toggle") {
        value = anim.toggle;
        return true;
    }

    if (fieldName == "bangPulse") {
        value = anim.bangPulse;
        return true;
    }

    return false;
}

bool TimelineDemoManager::readFloatField(const std::string& fieldName, float& value) const {
    if (!hasSceneEntity()) {
        return false;
    }

    const auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "value01") {
        value = anim.value01;
        return true;
    }

    return false;
}

bool TimelineDemoManager::readIntField(const std::string& fieldName, int& value) const {
    if (!hasSceneEntity()) {
        return false;
    }

    const auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);
    if (fieldName == "index08") {
        value = anim.index08;
        return true;
    }

    return false;
}

void TimelineDemoManager::writeBoolField(const std::string& fieldName, bool value) {
    if (!hasSceneEntity()) {
        return;
    }

    auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);

    if (fieldName == "toggle") {
        anim.toggle = value;
    } else if (fieldName == "bangPulse") {
        anim.bangPulse = value;
    }
}

void TimelineDemoManager::writeFloatField(const std::string& fieldName, float value) {
    if (!hasSceneEntity()) {
        return;
    }

    if (fieldName != "value01") {
        return;
    }

    auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);
    anim.value01 = ofClamp(value, 0.0f, 1.0f);
}

void TimelineDemoManager::writeIntField(const std::string& fieldName, int value) {
    if (!hasSceneEntity()) {
        return;
    }

    if (fieldName != "index08") {
        return;
    }

    auto& anim = registry_.get<DemoAnimComponent>(animatedEntity_);
    anim.index08 = ofClamp(value, 0, 8);
}

bool TimelineDemoManager::hasSceneEntity() const {
    return registry_.valid(animatedEntity_) && registry_.all_of<DemoAnimComponent>(animatedEntity_);
}

void ofApp::setup() {
    ofSetLogLevel(OF_LOG_NOTICE);
    ofSetVerticalSync(true);

    appLayoutPath_ = ofToDataPath("app_layout.json", true);
    loadWindowLayout();

    gui_.setup(nullptr, true, ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable);
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    setupParameters();

    timelineManager_.addParameter("toggle", pToggle_);
    timelineManager_.addParameter("value01", pValue01_);
    timelineManager_.addParameter("index08", pIndex08_);
    timelineManager_.addBangParameter("bangPulse", pBang_);
    timelineManager_.setup(ofToDataPath("imgui.ini", true));
}

void ofApp::update() { timelineManager_.update(ofGetLastFrameTime()); }

void ofApp::draw() {
    drawScene();

    gui_.begin();
    beginDockSpace();
    drawDemoScenePanel();
    timelineManager_.drawControlsAndTimeline();
    gui_.end();
}

void ofApp::exit() {
    saveWindowLayout();
    timelineManager_.exit();
}

void ofApp::setupParameters() {
    parameters_.setName("Demo Parameters");
    parameters_.add(pToggle_.set("Toggle On/Off", false));
    parameters_.add(pValue01_.set("Float 0..1", 0.15f, 0.0f, 1.0f));
    parameters_.add(pIndex08_.set("Int 0..8", 0, 0, 8));
    parameters_.add(pBang_.set("Bang"));
}

void ofApp::drawScene() {
    const bool toggle = timelineManager_.isToggleOn();
    const float value01 = timelineManager_.getValue01();
    const int index08 = timelineManager_.getIndex08();
    const float bangFlash01 = timelineManager_.getBangFlash01();

    const ofColor bgTop = toggle ? ofColor(18, 36, 52) : ofColor(34, 34, 36);
    const ofColor bgBottom = toggle ? ofColor(10, 20, 32) : ofColor(18, 18, 20);
    ofBackgroundGradient(bgTop, bgBottom, OF_GRADIENT_LINEAR);

    const float left = 70.0f;
    const float right = static_cast<float>(ofGetWidth()) - 70.0f;
    const float rowGap = 110.0f;
    const float rowStart = 110.0f;

    const ofColor accent = toggle ? ofColor(55, 220, 150) : ofColor(130, 138, 148);
    const ofColor inactive(75, 82, 90);

    ofPushStyle();
    ofSetColor(245);
    ofDrawBitmapString("Timeline-driven parameter demo", left, 42.0f);
    ofDrawBitmapString("1) Toggle  2) Float  3) Int  4) Bang", left, 62.0f);

    // 1) Toggle
    const float yToggle = rowStart;
    ofSetColor(220);
    ofDrawBitmapString("1) Toggle On/Off", left, yToggle - 18.0f);
    ofSetColor(52, 58, 64, 220);
    ofDrawRectRounded(right - 180.0f, yToggle - 24.0f, 160.0f, 48.0f, 10.0f);
    ofSetColor(accent);
    ofDrawCircle(right - (toggle ? 45.0f : 135.0f), yToggle, 16.0f);
    ofSetColor(250);
    ofDrawBitmapString(toggle ? "ON" : "OFF", right - 100.0f, yToggle + 4.0f);

    // 2) Float 0..1
    const float yFloat = rowStart + rowGap;
    ofSetColor(220);
    ofDrawBitmapString("2) Float 0..1", left, yFloat - 18.0f);
    ofSetColor(90, 98, 108, 200);
    ofSetLineWidth(3.0f);
    ofDrawLine(left, yFloat, right, yFloat);
    const float xFloat = ofMap(value01, 0.0f, 1.0f, left, right, true);
    ofSetColor(accent);
    ofDrawCircle(xFloat, yFloat, 16.0f);
    ofSetColor(245);
    ofDrawBitmapString(ofToString(value01, 2), right - 36.0f, yFloat - 12.0f);

    // 3) Int 0..8
    const float yInt = rowStart + rowGap * 2.0f;
    ofSetColor(220);
    ofDrawBitmapString("3) Int 0..8", left, yInt - 18.0f);
    for (int i = 0; i < 8; ++i) {
        const float x = ofMap(static_cast<float>(i), 0.0f, 7.0f, left, right);
        const bool enabled = i < index08;
        ofSetColor(enabled ? accent : inactive);
        ofDrawRectRounded(x - 18.0f, yInt - 16.0f, 28.0f, 32.0f, 5.0f);
    }
    ofSetColor(245);
    ofDrawBitmapString(ofToString(index08), right - 20.0f, yInt - 18.0f);

    // 4) Bang
    const float yBang = rowStart + rowGap * 3.0f;
    ofSetColor(220);
    ofDrawBitmapString("4) Bang pulse (rising edge)", left, yBang - 18.0f);
    const float bangRadius = 18.0f + value01 * 14.0f;
    ofSetColor(accent);
    ofNoFill();
    ofSetLineWidth(2.0f);
    ofDrawCircle(left + 30.0f, yBang, bangRadius);
    ofFill();

    if (bangFlash01 > 0.0f) {
        const float alpha = ofMap(bangFlash01, 0.0f, 1.0f, 0.0f, 190.0f, true);
        ofSetColor(255, 226, 90, alpha);
        ofDrawCircle(left + 30.0f, yBang, bangRadius + 18.0f * bangFlash01);
        ofSetColor(255, 226, 90, alpha * 0.22f);
        ofDrawRectangle(0.0f, 0.0f, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
    }

    ofSetColor(250);
    ofDrawBitmapString("bang count: " + ofToString(timelineManager_.getBangCount()), left + 70.0f, yBang + 5.0f);
    ofPopStyle();
}

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

    ImGui::Begin("##DockSpaceExampleBasic", nullptr, flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockId = ImGui::GetID("MainDockSpaceBasic");
    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    const bool forceRebuild = tanim::IsLayoutResetRequested();
    if (forceRebuild) {
        layoutBuilt_ = false;
        tanim::ClearLayoutResetRequest();
    }

    if (!layoutBuilt_) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockId);
        const bool hasSavedLayout = !forceRebuild && node && !node->IsLeafNode();
        if (!hasSavedLayout) {
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

            // Left: parameter / transport panel. Main: scene (top) + Tanim strip (bottom).
            ImGuiID left, mainArea;
            ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.28f, &left, &mainArea);

            ImGuiID editorStrip, sceneNode;
            ImGui::DockBuilderSplitNode(mainArea, ImGuiDir_Down, 0.42f, &editorStrip, &sceneNode);

            ImGui::DockBuilderDockWindow("Example Controls", left);
            ImGui::DockBuilderDockWindow("Demo Scene",      sceneNode);
            tanim::BuildEditorLayout(editorStrip);

            ImGui::DockBuilderFinish(dockId);
        }
        layoutBuilt_ = true;
    }

    ImGui::End();
}

void ofApp::drawDemoScenePanel() {
    ImGui::SetNextWindowSize(ImVec2(880.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Demo Scene", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    ImGui::TextDisabled("OpenGL demo output is drawn on the full window behind the UI.");
    ImGui::Spacing();
    ImGui::TextWrapped("Double-click a track's colored bar in the Timeliner (bottom) to expand the Keyframe Editor and Curves.");
    ImGui::End();
}

void ofApp::saveWindowLayout() const {
    ofJson json;
    json["x"] = ofGetWindowPositionX();
    json["y"] = ofGetWindowPositionY();
    json["w"] = ofGetWidth();
    json["h"] = ofGetHeight();

    if (!ofSavePrettyJson(appLayoutPath_, json)) {
        ofLogWarning("ofApp") << "Unable to save window layout to " << appLayoutPath_;
    }
}

void ofApp::loadWindowLayout() {
    ofFile file(appLayoutPath_);
    if (!file.exists()) {
        return;
    }

    try {
        const ofJson json = ofLoadJson(appLayoutPath_);
        int x = json.value("x", ofGetWindowPositionX());
        int y = json.value("y", ofGetWindowPositionY());
        int w = json.value("w", ofGetWidth());
        int h = json.value("h", ofGetHeight());

        w = std::max(640, w);
        h = std::max(480, h);
        x = ofClamp(x, 0, std::max(0, ofGetScreenWidth() - 80));
        y = ofClamp(y, 0, std::max(0, ofGetScreenHeight() - 80));

        ofSetWindowShape(w, h);
        ofSetWindowPosition(x, y);
    } catch (const std::exception& e) {
        ofLogWarning("ofApp") << "Unable to read app layout: " << e.what();
    }
}
