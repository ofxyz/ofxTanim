// REF: originally based on the imguizmo's example main.cpp:
// https://github.com/CedricGuillemet/ImGuizmo/blob/71f14292205c3317122b39627ed98efce137086a/example/main.cpp

#include "tanim/tanim_user.hpp"

#include "tanim/registry.hpp"
#include "tanim/timeliner.hpp"
#include "tanim/sequence.hpp"
#include "tanim/user_override.hpp"

#include "ofJson.h"

namespace tanim::internal
{
struct Impl
{
    TimelineData* m_editor_timeline_data{nullptr};
    ComponentData* m_editor_component_data{nullptr};
    std::vector<EntityData> m_editor_entity_datas{};
    entt::registry* m_editor_registry{nullptr};
    float m_snap_y_value = 0.1f;
    bool m_is_engine_in_play_mode{};
    bool m_preview{true};
    bool m_force_editor_timeline_frame{false};
    int m_forced_editor_timeline_frame{-1};
    ImGuiID m_editor_dock_node{0};
    bool    m_layout_reset_requested{false};
};

std::unique_ptr<Impl> g_impl{nullptr};

const RegisteredComponent* FindMatchingComponent(const Sequence& seq, const std::vector<EntityData>& entity_datas)
{
    const auto& components = GetRegistry().GetComponents();
    for (auto& component : components)
    {
        if (component.HasStructFieldName(seq.m_seq_id.StructFieldName()))
        {
            for (const auto& entity_data : entity_datas)
            {
                if (seq.m_seq_id.GetEntityData().m_uid == entity_data.m_uid)
                {
                    return &component;
                }
            }
        }
    }

    LogError("Couldn't find any entity with matching details: " + seq.m_seq_id.FullName());
    return nullptr;
}

void SetEditorTimelinePlayerFrame(int frame_num)
{
    if (g_impl->m_editor_timeline_data)
    {
        g_impl->m_forced_editor_timeline_frame = frame_num;
        g_impl->m_force_editor_timeline_frame = true;
    }
}

void Sample(entt::registry& registry, const std::vector<EntityData>& entity_datas, TimelineData& tdata, ComponentData& cdata)
{
    const int player_frame = Timeline::GetPlayerFrame(tdata, cdata);
    const float sample_time =
        Timeline::GetPlayerPlaying(cdata) ? Timeline::GetPlayerSampleTime(tdata, cdata) : static_cast<float>(player_frame);
    for (auto& seq : tdata.m_sequences)
    {
        if (!seq.IsRecording() && seq.IsBetweenFirstAndLastFrame(player_frame))
        {
            const auto* opt_comp = FindMatchingComponent(seq, entity_datas);
            if (opt_comp)
            {
                const auto opt_entity = Timeline::FindEntity(cdata, seq);
                if (opt_entity != entt::null)
                {
                    opt_comp->m_sample(registry, opt_entity, sample_time, seq);
                }
            }
        }
    }
}
}  // namespace tanim::internal

namespace tanim
{

using namespace tanim::internal;

void Init() { g_impl = std::make_unique<Impl>(); }

void EnterPlayMode() { g_impl->m_is_engine_in_play_mode = true; }

void ExitPlayMode() { g_impl->m_is_engine_in_play_mode = false; }

void UpdateEditor(float dt)
{
    if (!g_impl->m_is_engine_in_play_mode)
    {
        if (g_impl->m_editor_timeline_data != nullptr && g_impl->m_editor_component_data != nullptr)
        {
            TimelineData& tdata = *g_impl->m_editor_timeline_data;
            ComponentData& cdata = *g_impl->m_editor_component_data;
            if (Timeline::GetPlayerPlaying(cdata))
            {
                const bool has_passed_last_frame = Timeline::TickTime(tdata, cdata, dt);
                Sample(*g_impl->m_editor_registry, g_impl->m_editor_entity_datas, tdata, cdata);
                Timeline::CheckLooping(tdata, cdata, has_passed_last_frame);
            }
        }
    }
}

void OpenForEditing(entt::registry& registry,
                    const std::vector<EntityData>& entity_datas,
                    TimelineData& tdata,
                    ComponentData& cdata)
{
    g_impl->m_editor_timeline_data = &tdata;
    g_impl->m_editor_registry = &registry;
    g_impl->m_editor_entity_datas = entity_datas;
    g_impl->m_editor_component_data = &cdata;
}

void CloseEditor()
{
    g_impl->m_editor_timeline_data = nullptr;
    g_impl->m_editor_component_data = nullptr;
    g_impl->m_editor_registry = nullptr;
    g_impl->m_editor_entity_datas.clear();
}

void StartTimeline(const TimelineData& tdata, ComponentData& cdata)
{
    Timeline::ResetPlayerTime(cdata);
    if (Timeline::GetPlayImmediately(tdata))
    {
        Timeline::Play(cdata);
    }
}

void UpdateTimeline(entt::registry& registry,
                    const std::vector<EntityData>& entity_datas,
                    TimelineData& tdata,
                    ComponentData& cdata,
                    float delta_time)
{
    if (Timeline::GetPlayerPlaying(cdata))
    {
        const bool has_passed_last_frame = Timeline::TickTime(tdata, cdata, delta_time);
        Sample(registry, entity_datas, tdata, cdata);
        Timeline::CheckLooping(tdata, cdata, has_passed_last_frame);
    }
}

void StopTimeline(ComponentData& cdata) { Timeline::Stop(cdata); }

bool IsPlaying(const ComponentData& cdata) { return Timeline::GetPlayerPlaying(cdata); }

void Play(ComponentData& cdata) { Timeline::Play(cdata); }

void Pause(ComponentData& cdata) { Timeline::Pause(cdata); }

void Stop(ComponentData& cdata) { Timeline::Stop(cdata); }

void BuildEditorLayout(ImGuiID nodeId)
{
    // Store so ResetEditorLayout() can ask the app to rebuild.
    g_impl->m_editor_dock_node = nodeId;

    // Pure split-and-dock — no RemoveNode/AddNode/Finish.
    // This function must be called while a DockBuilder session is open on the
    // parent dockspace; the caller is responsible for DockBuilderFinish().
    // Left: Player info (top) + Curve inspector (bottom)
    ImGuiID edLeft, edCentreRight;
    ImGui::DockBuilderSplitNode(nodeId, ImGuiDir_Left, 0.15f, &edLeft, &edCentreRight);

    // Right: Timeline settings (top) + Keyframe Editor (bottom)
    ImGuiID edRight, edCentre;
    ImGui::DockBuilderSplitNode(edCentreRight, ImGuiDir_Right, 0.22f, &edRight, &edCentre);

    // Left column
    ImGuiID edPlayer, edCurves;
    ImGui::DockBuilderSplitNode(edLeft, ImGuiDir_Up, 0.35f, &edPlayer, &edCurves);

    // Right column
    ImGuiID edTimeline, edExpanded;
    ImGui::DockBuilderSplitNode(edRight, ImGuiDir_Up, 0.40f, &edTimeline, &edExpanded);

    // "timeliner" now contains both the transport bar (as a menu bar) and the lane editor.
    ImGui::DockBuilderDockWindow("timeliner",        edCentre);
    ImGui::DockBuilderDockWindow("Player",           edPlayer);
    ImGui::DockBuilderDockWindow("curves",           edCurves);
    ImGui::DockBuilderDockWindow("timeline",         edTimeline);
    ImGui::DockBuilderDockWindow("Keyframe Editor###expanded sequence", edExpanded);
}

void ResetEditorLayout()        { g_impl->m_layout_reset_requested = true; }
bool IsLayoutResetRequested()   { return g_impl->m_layout_reset_requested; }
void ClearLayoutResetRequest()  { g_impl->m_layout_reset_requested = false; }

void Draw()
{
    if (g_impl->m_editor_timeline_data == nullptr || g_impl->m_editor_component_data == nullptr)
    {
        return;
    }

    TimelineData& tdata = *g_impl->m_editor_timeline_data;
    ComponentData& cdata = *g_impl->m_editor_component_data;

    //*****************************************************

#pragma region timeliner
    {
        // Transport controls live in the menu bar — thin and always visible.
        ImGui::Begin("timeliner", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            // Play / Pause
            if (!Timeline::GetPlayerPlaying(cdata))
            {
                if (ImGui::Button("Play"))
                {
                    Timeline::ResetPlayerTime(cdata);
                    Timeline::Play(cdata);
                }
            }
            else
            {
                if (ImGui::Button("Pause"))
                    Timeline::Pause(cdata);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Preview toggle (disabled while playing)
            const bool playing = Timeline::GetPlayerPlaying(cdata);
            if (playing)
            {
                ImGui::BeginDisabled();
                g_impl->m_preview = true;
            }
            if (ImGui::Checkbox("Preview", &g_impl->m_preview))
            {
                if (!g_impl->m_preview)
                {
                    g_impl->m_preview = true;
                    const int frame_before = Timeline::GetPlayerFrame(tdata, cdata);
                    Timeline::ResetPlayerTime(cdata);
                    Sample(*g_impl->m_editor_registry, g_impl->m_editor_entity_datas, tdata, cdata);
                    g_impl->m_preview = false;
                    Timeline::SetPlayerTimeFromFrame(tdata, cdata, frame_before);
                }
            }
            if (playing)
                ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragInt("##Samples", &tdata.m_player_samples, 0.1f, Timeline::GetMinFrame(tdata));
            tdata.m_player_samples = ImMax(1, tdata.m_player_samples);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Samples per second");

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            int player_frame = Timeline::GetPlayerFrame(tdata, cdata);
            ImGui::SetNextItemWidth(60.0f);
            if (ImGui::InputInt("##Frame", &player_frame, 0, 0))
            {
                player_frame = ImMax(0, player_frame);
                Timeline::SetPlayerTimeFromFrame(tdata, cdata, player_frame);
                if (g_impl->m_preview)
                    Sample(*g_impl->m_editor_registry, g_impl->m_editor_entity_datas, tdata, cdata);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Current frame");

            ImGui::EndMenuBar();
        }

        // Timeline lane editor
        constexpr int timelinerFlags = TIMELINER_CHANGE_FRAME | TIMELINER_DELETE_SEQUENCE | TIMELINER_EDIT_STARTEND;

        int player_frame = Timeline::GetPlayerFrame(tdata, cdata);
        const int player_frame_before = player_frame;

        Timeliner(tdata, &player_frame, &tdata.m_expanded, &tdata.m_selected_sequence, &tdata.m_first_frame, timelinerFlags);

        int player_frame_after;
        if (g_impl->m_force_editor_timeline_frame)
        {
            player_frame_after = g_impl->m_forced_editor_timeline_frame;
            g_impl->m_force_editor_timeline_frame = false;
        }
        else
        {
            player_frame_after = player_frame;
        }

        if ((!g_impl->m_is_engine_in_play_mode && !Timeline::GetPlayerPlaying(cdata)) ||
            player_frame_before != player_frame_after)
        {
            Timeline::SetPlayerTimeFromFrame(tdata, cdata, player_frame_after);
            if (g_impl->m_preview)
                Sample(*g_impl->m_editor_registry, g_impl->m_editor_entity_datas, tdata, cdata);
        }

        ImGui::End();
    }
#pragma endregion

    //*****************************************************

#pragma region timeline

    ImGui::Begin("timeline");

    helpers::InspectEnum(tdata.m_playback_type);

    if (ImGui::Button("+ Sequence"))
    {
        ImGui::OpenPopup("AddSequencePopup");
    }

    if (ImGui::BeginPopup("AddSequencePopup"))
    {
        const auto& components = GetRegistry().GetComponents();
        for (const auto& entity_data : g_impl->m_editor_entity_datas)
        {
            for (const auto& component : components)
            {
                if (component.m_entity_has(*g_impl->m_editor_registry, Timeline::FindEntity(cdata, entity_data.m_uid)))
                {
                    for (const auto& field_name : component.m_field_names)
                    {
                        const std::string full_name =
                            helpers::MakeFullName(entity_data.m_uid, component.m_struct_name, field_name);

                        if (!Timeline::HasSequenceWithFullName(tdata, full_name))
                        {
                            const std::string display = entity_data.m_display + "::" +
                                                        helpers::MakeStructFieldName(component.m_struct_name, field_name);

                            if (ImGui::MenuItem(display.c_str()))
                            {
                                SequenceId seq_id{entity_data, component.m_struct_name, field_name};
                                component.m_add_sequence(*g_impl->m_editor_registry, tdata, cdata, seq_id);
                            }
                        }
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    char name_buf[256];
    strncpy_s(name_buf, Timeline::GetName(tdata).c_str(), sizeof(name_buf));
    if (ImGui::InputText("Timeline Name", name_buf, sizeof(name_buf)))
    {
        Timeline::SetName(tdata, std::string(name_buf));
    }

    ImGui::Checkbox("Play Immediately", &tdata.m_play_immediately);

    // ImGui::Text("max frame:            %d", Timeline::GetMaxFrame(tdata));
    // ImGui::Text("timeline last frame:  %d", Timeline::GetTimelineLastFrame(tdata));

    ImGui::End();

#pragma endregion

    //*****************************************************

#pragma region ExpandedSequence

    // "###expanded sequence" keeps the ini-file ID stable while showing a
    // friendlier display name in the tab bar.
    ImGui::Begin("Keyframe Editor###expanded sequence");

    bool has_expanded_seq = false;
    int expanded_seq_idx = -1;
    entt::entity expanded_seq_entity{entt::null};
    if (const auto idx = Timeline::GetExpandedSequenceIdx(tdata))
    {
        has_expanded_seq = true;
        expanded_seq_idx = idx.value();
        auto opt_entity = Timeline::FindEntity(tdata, cdata, expanded_seq_idx);
        expanded_seq_entity = opt_entity;
    }

    if (!has_expanded_seq)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("No track expanded.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Double-click the colored segment (clip bar) of a track in the Timeliner "
            "to expand it here and in Curves. Double-click again to collapse.");
    }

    if (has_expanded_seq)
    {
        Timeline::SetDrawMaxX(tdata, expanded_seq_idx, static_cast<float>(Timeline::GetMaxFrame(tdata)));
    }

    if (has_expanded_seq)
    {
        Sequence& seq = Timeline::GetSequence(tdata, expanded_seq_idx);
        const int player_frame = Timeline::GetPlayerFrame(tdata, cdata);
        const bool is_in_bounds = player_frame >= Timeline::GetSequenceFirstFrame(tdata, expanded_seq_idx) &&
                                  player_frame <= Timeline::GetSequenceLastFrame(tdata, expanded_seq_idx);
        const bool is_keyframe_in_all_curves = seq.IsKeyframeInAllCurves(player_frame);
        const bool disabled_new_keyframe = !is_in_bounds || is_keyframe_in_all_curves || Timeline::GetPlayerPlaying(cdata);
        if (disabled_new_keyframe)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("+Keyframe"))
        {
            seq.AddNewKeyframe(player_frame);
            seq.StartRecording(player_frame);

            const auto* opt_comp = FindMatchingComponent(seq, g_impl->m_editor_entity_datas);
            if (opt_comp)
            {
                opt_comp->m_record(*g_impl->m_editor_registry, expanded_seq_entity, seq.m_recording_frame, seq);
            }

            seq.StopRecording();
        }
        if (disabled_new_keyframe)
        {
            ImGui::EndDisabled();
        }

        bool disabled_delete_keyframe = player_frame <= 0 || player_frame >= Timeline::GetTimelineLastFrame(tdata) ||
                                        Timeline::GetPlayerPlaying(cdata) || !seq.IsKeyframeInAnyCurve(player_frame);
        if (disabled_delete_keyframe)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("-keyframe"))
        {
            seq.DeleteKeyframe(player_frame);
        }
        if (disabled_delete_keyframe)
        {
            ImGui::EndDisabled();
        }

        if (seq.IsRecording())
        {
            const bool clicked_on_stop_recording = ImGui::Button("Stop Recording");
            const bool has_moved_player_frame = Timeline::GetPlayerFrame(tdata, cdata) != seq.GetRecordingFrame();
            const bool has_moved_recording_frame_x = !seq.IsKeyframeInAllCurves(seq.GetRecordingFrame());
            if (clicked_on_stop_recording || has_moved_player_frame || has_moved_recording_frame_x)
            {
                seq.StopRecording();
            }
            else
            {
                const auto* opt_comp = FindMatchingComponent(seq, g_impl->m_editor_entity_datas);
                if (opt_comp)
                {
                    opt_comp->m_record(*g_impl->m_editor_registry, expanded_seq_entity, seq.m_recording_frame, seq);
                }
            }
        }
        else
        {
            const bool disabled_recording = !is_in_bounds || Timeline::GetPlayerPlaying(cdata);
            if (disabled_recording)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Record"))
            {
                seq.AddNewKeyframe(Timeline::GetPlayerFrame(tdata, cdata));
                seq.StartRecording(Timeline::GetPlayerFrame(tdata, cdata));
            }
            if (disabled_recording)
            {
                ImGui::EndDisabled();
            }
        }

        // ImGui::PushItemWidth(100);
        // ImGui::Text("index:       %d", expanded_seq_idx);
        // ImGui::BeginDisabled();
        // ImGui::DragFloat2("draw min", &seq.m_draw_min.x);
        // ImGui::DragFloat2("draw max", &seq.m_draw_max.x);
        // ImGui::EndDisabled();
        // ImGui::PopItemWidth();

        // ImGui::Text("seq first frame: %d", Timeline::GetSequenceFirstFrame(tdata, expanded_seq_idx));
        // ImGui::Text("seq last frame:  %d", Timeline::GetSequenceLastFrame(tdata, expanded_seq_idx));

        ImGui::Text(
            "entity:          %s",
            expanded_seq_entity == entt::null ? "NOT FOUND" : std::to_string(entt::to_integral(expanded_seq_entity)).c_str());
        ImGui::Text("uid:             %s", seq.m_seq_id.GetEntityData().m_uid.c_str());

        char uid_buf[256];
        strncpy_s(uid_buf, seq.m_seq_id.GetEntityData().m_uid.c_str(), sizeof(uid_buf));
        if (ImGui::InputText("uid", uid_buf, sizeof(uid_buf)))
        {
            seq.m_seq_id.SetUid(std::string(uid_buf));
        }

        ImGui::Text("display:         %s", seq.m_seq_id.GetEntityData().m_display.c_str());
        ImGui::Text("full name:       %s", seq.m_seq_id.FullName().c_str());

        // for (int i = 0; i < seq.GetCurveCount(); ++i)
        // {
        //     ImGui::PushID(i);
        //
        //     char curve_name_buf[256];
        //     strncpy_s(curve_name_buf, seq.m_curves.at(i).m_name.c_str(), sizeof(curve_name_buf));
        //     if (ImGui::InputText("Field", curve_name_buf, sizeof(curve_name_buf)))
        //     {
        //         seq.m_curves.at(i).m_name = std::string(curve_name_buf);
        //     }
        //
        //     ImGui::PopID();
        // }
    }

    ImGui::End();

#pragma endregion

    //*****************************************************

#pragma region curves

    ImGui::Begin("curves");

    if (!has_expanded_seq)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("No track expanded.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Double-click a track's colored segment in the Timeliner to expand it; "
            "then this panel shows the inspector for that sequence.");
    }

    if (has_expanded_seq)
    {
        Sequence& seq = Timeline::GetSequence(tdata, expanded_seq_idx);
        const bool is_recording = seq.IsRecording();
        if (!seq.IsRecording())
        {
            const auto* opt_comp = FindMatchingComponent(seq, g_impl->m_editor_entity_datas);
            if (opt_comp)
            {
                if (is_recording)
                {
                    ImGui::Text("RECORDING");
                    ImGui::BeginDisabled();
                }

                ImGui::Text("%s", opt_comp->m_struct_name.c_str());
                opt_comp->m_inspect(*g_impl->m_editor_registry,
                                    expanded_seq_entity,
                                    Timeline::GetPlayerFrame(tdata, cdata),
                                    seq,
                                    IsPlaying(cdata));
                ImGui::Separator();

                if (is_recording)
                {
                    ImGui::EndDisabled();
                }
            }
        }
    }

    ImGui::End();

#pragma endregion

    //*****************************************************

#pragma region player

    ImGui::Begin("Player");

    ImGui::Text("Real Time:    %.3fs", Timeline::GetPlayerRealTime(cdata));
    ImGui::Text("Sample Time:  %.3fs", Timeline::GetPlayerSampleTime(tdata, cdata));
    ImGui::Text("Frame:        %d", Timeline::GetPlayerFrame(tdata, cdata));

    ImGui::End();

#pragma endregion

    //*****************************************************
}

std::string Serialize(TimelineData& tdata)
{
    nlohmann::ordered_json json{};

    json["about"] = {"Tanim Serialized JSON", "more info: https://github.com/hegworks/tanim"};

    json["version"] = 2;
    /*
     * version history:
     * 1:
     *     varying curves: smoothstep, linear, constant
     * 2: NOT backward compatible
     *     removed varying curves, replaced all of them with bezier curves
     *     added handles (control points) for bezier curves
     */

    nlohmann::ordered_json timeline_js{};
    timeline_js["m_name"] = tdata.m_name;
    timeline_js["m_first_frame"] = tdata.m_first_frame;
    timeline_js["m_last_frame"] = tdata.m_last_frame;
    timeline_js["m_min_frame"] = tdata.m_min_frame;
    timeline_js["m_max_frame"] = tdata.m_max_frame;
    timeline_js["m_play_immediately"] = tdata.m_play_immediately;
    timeline_js["m_player_samples"] = tdata.m_player_samples;
    timeline_js["m_playback_type"] = std::string(magic_enum::enum_name(tdata.m_playback_type));

    nlohmann::ordered_json sequences_js_array = nlohmann::ordered_json::array();
    for (int seq_idx = 0; seq_idx < static_cast<int>(tdata.m_sequences.size()); ++seq_idx)
    {
        nlohmann::ordered_json seq_js{};

        const Sequence& seq = tdata.m_sequences.at(seq_idx);

        nlohmann::ordered_json seq_id_js{};
        seq_id_js["m_entity_data"]["m_uid"] = seq.m_seq_id.GetEntityData().m_uid;
        seq_id_js["m_entity_data"]["m_display"] = seq.m_seq_id.GetEntityData().m_display;
        seq_id_js["m_struct_name"] = seq.m_seq_id.StructName();
        seq_id_js["m_field_name"] = seq.m_seq_id.FieldName();
        seq_js["m_seq_id"] = seq_id_js;

        seq_js["m_type_meta"] = std::string(magic_enum::enum_name(seq.m_type_meta));
        seq_js["m_representation_meta"] = std::string(magic_enum::enum_name(seq.m_representation_meta));
        seq_js["m_last_frame"] = seq.m_last_frame;
        seq_js["m_first_frame"] = seq.m_first_frame;

        nlohmann::ordered_json curves_js_array = nlohmann::ordered_json::array();
        for (int curve_idx = 0; curve_idx < seq.GetCurveCount(); ++curve_idx)
        {
            nlohmann::ordered_json curve_js{};

            const Curve& curve = seq.m_curves.at(curve_idx);
            curve_js["m_name"] = curve.m_name;
            curve_js["m_handle_type_locked"] = curve.m_handle_type_locked;
            curve_js["m_curve_handle_type"] = std::string(magic_enum::enum_name(curve.m_curve_handle_type));

            nlohmann::ordered_json kfs_js_array = nlohmann::ordered_json::array();
            int keyframe_count = GetKeyframeCount(curve);
            for (int k = 0; k < keyframe_count; ++k)
            {
                nlohmann::ordered_json kf_js{};

                const auto& keyframe = curve.m_keyframes.at(k);
                kf_js["m_pos"] = {keyframe.m_pos.x, keyframe.m_pos.y};
                kf_js["m_handle_type"] = std::string(magic_enum::enum_name(keyframe.m_handle_type));

                auto serialize_handle = [](nlohmann::ordered_json::reference js, const Handle& handle)
                {
                    js["m_offset"] = {handle.m_offset.x, handle.m_offset.y};
                    js["m_weighted"] = handle.m_weighted;
                    js["m_smooth_type"] = std::string(magic_enum::enum_name(handle.m_smooth_type));
                    js["m_broken_type"] = std::string(magic_enum::enum_name(handle.m_broken_type));
                };
                nlohmann::ordered_json in_js{};
                serialize_handle(in_js, keyframe.m_in);
                nlohmann::ordered_json out_js{};
                serialize_handle(out_js, keyframe.m_out);

                kf_js["m_in"] = in_js;
                kf_js["m_out"] = out_js;
                kfs_js_array.push_back(kf_js);
            }
            curve_js["m_keyframes"] = kfs_js_array;

            curves_js_array.push_back(curve_js);
        }

        seq_js["m_curves"] = curves_js_array;
        sequences_js_array.push_back(seq_js);
    }

    timeline_js["m_sequences"] = sequences_js_array;
    json["timeline_data"] = timeline_js;

    return json.dump(2);
}

void Deserialize(TimelineData& tdata, const std::string& serialized_string)
{
    assert(!serialized_string.empty());
    const nlohmann::ordered_json json = nlohmann::ordered_json::parse(serialized_string);
    assert(!json.empty());

    const int version = json.value("version", 1);
    if (version < 2)
    {
        LogError("Versions prior to 2 are not supported. Can not deserialize.");
        return;
    }

    const auto& timeline_js = json.at("timeline_data");
    tdata.m_name = timeline_js.at("m_name").get<std::string>();
    tdata.m_first_frame = timeline_js.at("m_first_frame").get<int>();
    tdata.m_last_frame = timeline_js.at("m_last_frame").get<int>();
    tdata.m_min_frame = timeline_js.at("m_min_frame").get<int>();
    tdata.m_max_frame = timeline_js.at("m_max_frame").get<int>();
    tdata.m_play_immediately = timeline_js.at("m_play_immediately").get<bool>();
    tdata.m_player_samples = timeline_js.at("m_player_samples").get<int>();

    const std::string playback_type_str = timeline_js.at("m_playback_type").get<std::string>();
    tdata.m_playback_type = magic_enum::enum_cast<PlaybackType>(playback_type_str).value_or(PlaybackType::HOLD);

    tdata.m_sequences.clear();
    for (const auto& seq_js : timeline_js.at("m_sequences"))
    {
        Sequence& seq = tdata.m_sequences.emplace_back();

        const auto& seq_id_js = seq_js.at("m_seq_id");
        auto uid = seq_id_js.at("m_entity_data").at("m_uid").get<std::string>();
        auto display = seq_id_js.at("m_entity_data").at("m_display").get<std::string>();
        auto struct_name = seq_id_js.at("m_struct_name").get<std::string>();
        auto field_name = seq_id_js.at("m_field_name").get<std::string>();
        seq.m_seq_id = SequenceId(EntityData{uid, display}, struct_name, field_name);

        const std::string type_meta_str = seq_js.at("m_type_meta").get<std::string>();
        seq.m_type_meta = magic_enum::enum_cast<Sequence::TypeMeta>(type_meta_str).value_or(Sequence::TypeMeta::NONE);

        const std::string representation_meta_str = seq_js.at("m_representation_meta").get<std::string>();
        seq.m_representation_meta =
            magic_enum::enum_cast<RepresentationMeta>(representation_meta_str).value_or(RepresentationMeta::NONE);

        seq.m_last_frame = seq_js.at("m_last_frame").get<int>();
        seq.m_first_frame = seq_js.at("m_first_frame").get<int>();

        seq.m_curves.clear();
        for (const auto& curve_js : seq_js.at("m_curves"))
        {
            Curve& curve = seq.m_curves.emplace_back();

            curve.m_name = curve_js.at("m_name").get<std::string>();
            curve.m_handle_type_locked = curve_js.at("m_handle_type_locked").get<bool>();

            const std::string curve_handle_type_str = curve_js.at("m_curve_handle_type").get<std::string>();
            curve.m_curve_handle_type =
                magic_enum::enum_cast<CurveHandleType>(curve_handle_type_str).value_or(CurveHandleType::UNCONSTRAINED);

            curve.m_keyframes.clear();
            for (const auto& kf_js : curve_js.at("m_keyframes"))
            {
                const auto& pos_arr = kf_js.at("m_pos");
                Keyframe& kf = curve.m_keyframes.emplace_back(pos_arr.at(0).get<float>(), pos_arr.at(1).get<float>());

                const std::string handle_type_str = kf_js.at("m_handle_type").get<std::string>();
                kf.m_handle_type = magic_enum::enum_cast<HandleType>(handle_type_str).value_or(HandleType::SMOOTH);

                auto deserialize_handle = [](const nlohmann::ordered_json& js, Handle& handle)
                {
                    const auto& offset_arr = js.at("m_offset");
                    handle.m_offset.x = offset_arr.at(0).get<float>();
                    handle.m_offset.y = offset_arr.at(1).get<float>();
                    handle.m_weighted = js.at("m_weighted").get<bool>();

                    const std::string smooth_type_str = js.at("m_smooth_type").get<std::string>();
                    handle.m_smooth_type =
                        magic_enum::enum_cast<Handle::SmoothType>(smooth_type_str).value_or(Handle::SmoothType::AUTO);

                    const std::string broken_type_str = js.at("m_broken_type").get<std::string>();
                    handle.m_broken_type =
                        magic_enum::enum_cast<Handle::BrokenType>(broken_type_str).value_or(Handle::BrokenType::UNUSED);
                };

                deserialize_handle(kf_js.at("m_in"), kf.m_in);
                deserialize_handle(kf_js.at("m_out"), kf.m_out);
            }
        }
    }

    Timeline::RefreshTimelineLastFrame(tdata);
}
}  // namespace tanim
