#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ofMain.h"
#include "ofxImGui.h"
#include "ofxTanim.h"

// ─────────────────────────────────────────────────────────────────────────────
// SurfingTimelineManager
// Bridges ofParameterGroup members to a Tanim timeline.
// ─────────────────────────────────────────────────────────────────────────────
class SurfingTimelineManager {
public:
    using BoolToggleCallback = std::function<void(const std::string& name, bool value)>;
    using BangCallback       = std::function<void(const std::string& name)>;

    void addParameters(ofParameterGroup& parameters);
    void setBoolToggleCallback(BoolToggleCallback callback);
    void setBangCallback(BangCallback callback);

    void setup(const std::string& imguiIniPath);
    void update(float deltaTime);
    void drawControlsAndTimeline();
    void exit();

    bool      isReady()        const;
    glm::vec3 getPosition()    const;
    glm::vec3 getRotationDeg() const;
    bool      isToggleEnabled() const;

private:
    enum class TrackKind { Position, Rotation, Toggle, BangPulse };

    struct TrackSpec {
        std::string fieldName;
        TrackKind   kind{TrackKind::Position};
    };

    struct Vec3Binding {
        std::string              fieldName;
        std::string              parameterName;
        ofParameter<glm::vec3>*  parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    struct BoolBinding {
        std::string             fieldName;
        std::string             parameterName;
        ofParameter<bool>*      parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
    };

    struct BangBinding {
        std::string             fieldName;
        std::string             parameterName;
        ofParameter<void>*      parameter{nullptr};
        std::unique_ptr<of::priv::AbstractEventToken> listener;
        bool                    previousPulse{false};
    };

    void scanGroup(ofParameterGroup& group);
    void addVec3Parameter(ofParameter<glm::vec3>& parameter);
    void addBoolParameter(ofParameter<bool>& parameter);
    void addBangParameter(ofParameter<void>& parameter);

    std::string normalizeName(const std::string& name) const;
    std::string resolveVec3Field(const std::string& parameterName) const;
    std::string resolveBoolField(const std::string& parameterName) const;
    std::string resolveBangField(const std::string& parameterName) const;

    bool hasVec3Field(const std::string& fieldName) const;
    bool hasBoolField(const std::string& fieldName) const;
    bool hasBangField(const std::string& fieldName) const;

    void registerTrack(const std::string& fieldName, TrackKind kind);

    void setupScene();
    void setupTimeline();
    void syncParametersFromComponent();

    void onVec3ParameterChanged(const std::string& fieldName, const glm::vec3& value);
    void onBoolParameterChanged(const std::string& fieldName, const std::string& parameterName, bool value);
    void onBangParameterTriggered(const std::string& parameterName);

    bool readVec3Field(const std::string& fieldName, glm::vec3& value) const;
    bool readBoolField(const std::string& fieldName, bool& value)       const;
    void writeVec3Field(const std::string& fieldName, const glm::vec3& value);
    void writeBoolField(const std::string& fieldName, bool value);

    bool hasSceneEntity() const;

    bool        setupDone_{false};
    bool        isPlayMode_{false};
    bool        isSyncingFromAnimation_{false};
    bool        pendingLateIniReload_{false};
    std::string imguiIniPath_;

    ofParameterGroup*       parametersGroup_{nullptr};
    std::vector<Vec3Binding>  vec3Bindings_;
    std::vector<BoolBinding>  boolBindings_;
    std::vector<BangBinding>  bangBindings_;
    std::vector<TrackSpec>    trackSpecs_;

    BoolToggleCallback boolToggleCallback_;
    BangCallback       bangCallback_;

    entt::registry                registry_;
    entt::entity                  animatedEntity_{entt::null};
    std::vector<tanim::EntityData> entityDatas_;
    tanim::TimelineData           timelineData_;
    tanim::ComponentData          componentData_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ofApp
// ─────────────────────────────────────────────────────────────────────────────
class ofApp : public ofBaseApp {
public:
    void setup()  override;
    void update() override;
    void draw()   override;
    void exit()   override;
    void windowResized(int w, int h) override;

private:
    void setupParameters();
    void setupImGui();

    // DPI / scale
    float computeUiScale() const;

    // Viewport FBO
    void resizeViewport(int w, int h);
    void renderSceneToViewport();
    void drawScene3d();

    // ImGui panels
    void beginDockSpace();
    void drawViewportWindow();
    void drawCallbacksWindow();

    // Callbacks from SurfingTimelineManager
    void onBoolToggle(const std::string& name, bool value);
    void onBang(const std::string& name);

    ofxImGui::Gui gui_;
    ofEasyCam     camera_;
    ofFbo         viewport_;
    glm::vec2     viewportSize_{0.0f, 0.0f};
    /// Cached `DisplayFramebufferScale` while the Viewport window is submitting (same-frame FBO sizing).
    glm::vec2     viewportFbScale_{1.0f, 1.0f};
    bool          layoutBuilt_{false};

    ofParameterGroup    parameters_;
    ofParameter<glm::vec3> pPosition_;
    ofParameter<glm::vec3> pRotationDeg_;
    ofParameter<bool>   pToggle_;
    ofParameter<void>   pBang_;

    SurfingTimelineManager timelineManager_;

    float         bangFlash_{0.0f};
    std::uint64_t bangCounter_{0};
    std::string   lastBoolEvent_{"-"};
    std::string   lastBangEvent_{"-"};
};
