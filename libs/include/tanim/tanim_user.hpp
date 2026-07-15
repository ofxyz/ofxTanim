#pragma once

#include <imgui.h>

#include "tanim/registry.hpp"
#include "tanim/user_data.hpp"

namespace tanim
{

/// Call only once, at the start of your application.
/// Initializes Tanim.
void Init();

/// Call once every frame, between your application's ImGui::NewFrame() & ImGui::EndFrame().
/// Draws all Tanim editor sub-windows (controls, timeliner, Player, curves, timeline,
/// expanded sequence). Each window is a plain, free-floating ImGui window that can be
/// docked into any dockspace — typically the application's main dockspace.
void Draw();

/// Dock all Tanim editor sub-windows into @p nodeId using the built-in default split layout.
/// Call this once during initial setup (when no imgui.ini exists), and whenever the user
/// wants to reset the editor layout back to defaults.
/// @param nodeId  Any ImGui dockspace node ID — usually a sub-node you've already carved
///                out of your main dockspace with DockBuilderSplitNode().
void BuildEditorLayout(ImGuiID nodeId);

/// Request a layout reset. Sets an internal flag that the app should check each
/// frame (via IsLayoutResetRequested()) and respond to by rebuilding its dockspace
/// with BuildEditorLayout(). Cleared by ClearLayoutResetRequest().
void ResetEditorLayout();

/// Returns true if ResetEditorLayout() has been called and the app has not yet
/// cleared the request.
bool IsLayoutResetRequested();

/// Call after the app has honoured a layout reset request.
void ClearLayoutResetRequest();

/// Call once every frame, with your other ECS system updates.
/// Updates the data in the Tanim editor window.
/// @param dt Delta Time of your application
void UpdateEditor(float dt);

/// Call once, when you want to open the Tanim editor window to edit a timeline.
/// @param registry Your main (current scene's) entt registry. Used to 'get' the reflected component on the entity you are
/// animating, to read/write to its animated fields during various tanim operations (add sequence, sample, record, etc.)
/// @param entity_datas A list of all the entities you want to be able to animate in this timeline. For example, it can include
/// the entity that has the timeline, and all of its children recursively. Note that instead of a vector of entt::entity,
/// you have to send a vector of tanim::EntityData for each entt::entity you want to send. Check the EntityData docs for more
/// info on it.
/// @param tdata TimelineData of the entity. Check TimelineData docs for more info.
/// @param cdata ComponentData of the entity. Check ComponentData docs for more info.
void OpenForEditing(entt::registry& registry,
                    const std::vector<EntityData>& entity_datas,
                    TimelineData& tdata,
                    ComponentData& cdata);

/// Call once, when you want to close the Tanim editor window opened by tanim::OpenForEditing. Use when you no longer have the
/// opened timeline's TimelineData or ComponentData in the memory. For example, when removing the Tanim component from an
/// entity, deleting its TimelineData, or changing to a new scene (with a new registry).
void CloseEditor();

/// Call once, when your application enters "play" mode. Usually when you "start" your other ECS systems. For example, everytime
/// the engine goes from "edit" mode to "play" mode, or once, when the application launches in "release" mode.
void EnterPlayMode();

/// Call once for every entity that has the Tanim component, after you call tanim::EnterPlayMode.
/// @param tdata TimelineData associated with this entity. Check TimelineData docs for more info.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
void StartTimeline(const TimelineData& tdata, ComponentData& cdata);

/// Call every frame for every entity that has the Tanim component, only during the "play" mode. Which is every frame between
/// the user calls tanim::EnterPlayMode & tanim::ExitPlayMode.
/// @param registry Your main (current scene's) entt registry. Used to 'get' the reflected component on the entity you are
/// animating, to read/write to its animated fields during various tanim operations (add sequence, sample, record, etc.)
/// @param entity_datas A list of all the entities you wanted to be able to animate in this timeline. Usually the same vector
/// you sent to tanim::OpenForEditing when you wanted to open the timeline for editing. Check tanim::OpenForEditing doc of the
/// entity_datas for more info.
/// @param tdata TimelineData of the entity. Check TimelineData docs for more info.
/// @param cdata ComponentData of the entity. Check ComponentData docs for more info.
/// @param delta_time Delta Time of your application.
void UpdateTimeline(entt::registry& registry,
                    const std::vector<EntityData>& entity_datas,
                    TimelineData& tdata,
                    ComponentData& cdata,
                    float delta_time);

/// Call once for every entity that has the Tanim component, before you call tanim::ExitPlayMode.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
void StopTimeline(ComponentData& cdata);

/// Call once, when your application exits "play" mode. Usually when you "stop" your other ECS systems. For example, everytime
/// the engine goes from "play" mode to "edit" mode.
void ExitPlayMode();

/// Call whenever you want to check if the Tanim component on the entity is in play mode.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
bool IsPlaying(const ComponentData& cdata);

/// Call whenever you want to make the Tanim component on the entity enter play mode.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
void Play(ComponentData& cdata);

/// Call whenever you want to make the Tanim component on the entity enter pause state. When you call tanim::Play afterward, it
/// will continue from the time that you paused it.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
void Pause(ComponentData& cdata);

/// Call whenever you want to make the Tanim component on the entity stop playing. When you call tanim::Play afterward, it will
/// continue from beginning of the timeline.
/// @param cdata ComponentData associated with this entity. Check ComponentData docs for more info.
void Stop(ComponentData& cdata);

/// It will give you the serialized string of all the data that needs to be preserved for restoring the string back to
/// TimelineData. You can call it for example when you want to save the TimelineData on the disk, or you want to store it on the
/// memory for an undo/redo system.
/// @param tdata The TimelineData you want the serialized string of.
/// @return A string that when passed to tanim::Deserialize, will restore the TimelineData with the data in the string.
[[nodiscard]] std::string Serialize(TimelineData& tdata);

/// It will restore the data inside serialized_string onto tdata. You can call ilt for example when you want to load the
/// TimelineData from the file containing the serialized_string you had saved disk, or when you want to load it from the memory
/// from your undo/redo system.
/// @param tdata The TimelineData that you want to be filled with the data from the serialized_string
/// @param serialized_string The string that was given to you by tanim::Serialize
void Deserialize(TimelineData& tdata, const std::string& serialized_string);

/// Register a component type so Tanim can animate its fields.
///
/// Before calling this, specialise inspector::registerProperties<T> to describe
/// which fields are animatable (see ComponentInspector.h from ofxEnTTInspector).
///
/// @tparam T          The component struct type to register.
/// @param structName  Stable name stored in serialized JSON. Providing an
///                    explicit human-readable string is strongly recommended so
///                    that saved timelines survive refactors. Defaults to the
///                    compiler's typeid name if omitted.
template <typename T>
void RegisterComponent(const std::string& structName = typeid(T).name())
{
    internal::GetRegistry().RegisterComponent<T>(structName);
}

}  // namespace tanim
