#pragma once

#ifdef VISIT_STRUCT_PP_HAS_VA_OPT
#undef VISIT_STRUCT_PP_HAS_VA_OPT
#endif
#define VISIT_STRUCT_PP_HAS_VA_OPT 0

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <ofMain.h>
#include <ofxImGui.h>
#include <ofxTanim.h>

class TimelineDemoManager {
public:
    void addParameter(const std::string& fieldName, ofParameter<bool>& parameter);
    void addParameter(const std::string& fieldName, ofParameter<float>& parameter);
    void addParameter(const std::string& fieldName, ofParameter<int>& parameter);
    void addBangParameter(const std::string& fieldName, ofParameter<void>& parameter);

    void setup(const std::string& imguiIniPath);
    void update(float deltaTime);
    void drawControlsAndTimeline();
    void exit();

    bool isReady() const;
    bool isToggleOn() const;
    float getValue01() const;
    int getIndex08() const;
    float getBangFlash01() const;
    std::uint64_t getBangCount() const;

private:
    struct BoolBinding {
        std::string fieldName;
        ofParameter<bool>* parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    struct FloatBinding {
        std::string fieldName;
        ofParameter<float>* parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    struct IntBinding {
        std::string fieldName;
        ofParameter<int>* parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    struct BangBinding {
        std::string fieldName;
        ofParameter<void>* parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
        bool previousPulse{false};
    };

    enum class TrackKind {
        Toggle,
        Float01,
        Int08,
        BangPulse
    };

    struct TrackSpec {
        std::string fieldName;
        TrackKind kind{TrackKind::Toggle};
    };

    void registerTrack(const std::string& fieldName, TrackKind kind);

    void setupScene();
    void setupTimeline();

    void onBoolParameterChanged(const std::string& fieldName, bool value);
    void onFloatParameterChanged(const std::string& fieldName, float value);
    void onIntParameterChanged(const std::string& fieldName, int value);
    void onBangParameterTriggered();

    void syncParametersFromComponent();

    void triggerBangFeedback();

    bool readBoolField(const std::string& fieldName, bool& value) const;
    bool readFloatField(const std::string& fieldName, float& value) const;
    bool readIntField(const std::string& fieldName, int& value) const;

    void writeBoolField(const std::string& fieldName, bool value);
    void writeFloatField(const std::string& fieldName, float value);
    void writeIntField(const std::string& fieldName, int value);

    bool hasSceneEntity() const;

    bool setupDone_{false};
    bool isPlayMode_{false};
    bool isSyncingFromAnimation_{false};
    bool pendingLateIniReload_{false};

    float bangFlashTimer_{0.0f};
    std::uint64_t bangCounter_{0};

    std::vector<BoolBinding> boolBindings_;
    std::vector<FloatBinding> floatBindings_;
    std::vector<IntBinding> intBindings_;
    std::vector<BangBinding> bangBindings_;
    std::vector<TrackSpec> trackSpecs_;

    entt::registry registry_;
    entt::entity animatedEntity_{entt::null};
    std::vector<tanim::EntityData> entityDatas_;
    tanim::TimelineData timelineData_;
    tanim::ComponentData componentData_;

    std::string imguiIniPath_;
};

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;

private:
    void setupParameters();
    void drawScene();
    void beginDockSpace();
    void drawDemoScenePanel();

    void saveWindowLayout() const;
    void loadWindowLayout();

    ofxImGui::Gui gui_;

    ofParameterGroup parameters_;
    ofParameter<bool> pToggle_;
    ofParameter<float> pValue01_;
    ofParameter<int> pIndex08_;
    ofParameter<void> pBang_;

    TimelineDemoManager timelineManager_;

    std::string appLayoutPath_;
    bool layoutBuilt_{false};
};
