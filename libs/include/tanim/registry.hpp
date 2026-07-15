#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Tanim Registry — refactored to use ofxEnTTInspector's reflection system.
//
// Components register themselves by specializing inspector::registerProperties<T>
// (from ofxEnTTInspector's ComponentInspector.h). This single specialization is
// then shared by the Inspector UI, Tanim animation, and ofxEnTTStateCollector.
//
// MIGRATION from TANIM_REFLECT:
//   Old: TANIM_REFLECT(my_comp, x, y, z);  // at global scope
//        tanim::RegisterComponent<my_comp>();
//
//   New: // In a header, specialise once:
//        template<> inline void inspector::registerProperties(
//            my_comp& c, inspector::ComponentInspector& ci)
//        {
//            ci.addProperty("x", &c.x, -100.f, 100.f);
//            ci.addProperty("y", &c.y, -100.f, 100.f);
//            ci.addProperty("z", &c.z, -100.f, 100.f);
//        }
//        // Then register with an explicit, stable name:
//        tanim::RegisterComponent<my_comp>("my_comp");
// ─────────────────────────────────────────────────────────────────────────────

#include "tanim/timeline.hpp"
#include "tanim/enums.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ofxEnTTInspector — provides ReflectedProperty, ComponentInspector, PinDataType
#include "ComponentInspector.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace tanim::internal
{

// ─────────────────────────────────────────────────────────────────────────────
// Non-template reflection helpers
// All operate on a vector<ReflectedProperty> obtained from ComponentInspector.
// ─────────────────────────────────────────────────────────────────────────────

namespace reflection
{

inline void AddSequence(const std::vector<inspector::ReflectedProperty>& props,
                        const std::string&                                fieldName,
                        TimelineData&                                     tdata,
                        SequenceId&                                       seq_id)
{
    for (const auto& prop : props)
    {
        if (prop.name != fieldName) continue;

        Sequence& seq      = Timeline::AddSequenceStatic(tdata);
        seq.m_seq_id       = seq_id;
        seq.m_last_frame   = Timeline::GetTimelineLastFrame(tdata);
        const float lf     = static_cast<float>(seq.m_last_frame);

        auto addCurve = [&](const std::string& name, float initVal,
                            CurveHandleType cht = CurveHandleType::UNCONSTRAINED,
                            bool locked = false)
        {
            Curve& c = seq.AddCurve();
            c.m_name = name;
            if (cht != CurveHandleType::UNCONSTRAINED) SetCurveHandleType(c, cht);
            if (locked) LockCurveHandleType(c);
            const int ci = seq.GetCurveCount() - 1;
            seq.AddKeyframeAtPos(ci, {0.f,  initVal});
            seq.AddKeyframeAtPos(ci, {lf,   initVal});
        };

        switch (prop.type)
        {
            case inspector::PinDataType::Float:
                addCurve(fieldName, *prop.asFloat());
                break;

            case inspector::PinDataType::Int:
                seq.m_type_meta = Sequence::TypeMeta::INT;
                addCurve(fieldName, static_cast<float>(*prop.asInt()),
                         CurveHandleType::LINEAR, true);
                break;

            case inspector::PinDataType::Bool:
                seq.m_type_meta = Sequence::TypeMeta::BOOL;
                addCurve(fieldName, *prop.asBool() ? 1.f : 0.f,
                         CurveHandleType::CONSTANT, true);
                break;

            case inspector::PinDataType::Vec2:
            {
                auto* v = prop.asVec2();
                addCurve("X", v->x);
                addCurve("Y", v->y);
                break;
            }
            case inspector::PinDataType::Vec3:
            {
                seq.m_representation_meta = RepresentationMeta::VECTOR;
                auto* v = prop.asVec3();
                addCurve("X", v->x);
                addCurve("Y", v->y);
                addCurve("Z", v->z);
                break;
            }
            case inspector::PinDataType::Vec4:
            {
                auto* v = prop.asVec4();
                addCurve("X", v->x);
                addCurve("Y", v->y);
                addCurve("Z", v->z);
                addCurve("W", v->w);
                break;
            }
            case inspector::PinDataType::Quat:
            {
                seq.m_representation_meta = RepresentationMeta::QUAT;
                auto* v = prop.asQuat();
                addCurve("W", v->w, CurveHandleType::LINEAR, true);
                addCurve("X", v->x, CurveHandleType::LINEAR, true);
                addCurve("Y", v->y, CurveHandleType::LINEAR, true);
                addCurve("Z", v->z, CurveHandleType::LINEAR, true);
                addCurve("Spins", 0.f, CurveHandleType::CONSTANT, true);
                break;
            }
            case inspector::PinDataType::Color:
            {
                seq.m_representation_meta = RepresentationMeta::COLOR;
                auto* col = prop.asColor();
                addCurve("R", col->r / 255.f);
                addCurve("G", col->g / 255.f);
                addCurve("B", col->b / 255.f);
                addCurve("A", col->a / 255.f);
                break;
            }
            default:
                break;  // String, Trigger, Any are not animatable
        }
        return;
    }
}

inline void Sample(const std::vector<inspector::ReflectedProperty>& props,
                   const std::string&                                fieldName,
                   float                                             sampleTime,
                   Sequence&                                         seq)
{
    for (const auto& prop : props)
    {
        if (prop.name != fieldName) continue;

        switch (prop.type)
        {
            case inspector::PinDataType::Float:
                *prop.asFloat() = SampleCurveValue(seq.m_curves.at(0), sampleTime);
                break;
            case inspector::PinDataType::Int:
                *prop.asInt() = static_cast<int>(
                    std::floorf(SampleCurveValue(seq.m_curves.at(0), sampleTime)));
                break;
            case inspector::PinDataType::Bool:
                *prop.asBool() = std::roundf(
                    SampleCurveValue(seq.m_curves.at(0), sampleTime)) >= 0.5f;
                break;
            case inspector::PinDataType::Vec2:
                prop.asVec2()->x = SampleCurveValue(seq.m_curves.at(0), sampleTime);
                prop.asVec2()->y = SampleCurveValue(seq.m_curves.at(1), sampleTime);
                break;
            case inspector::PinDataType::Vec3:
                prop.asVec3()->x = SampleCurveValue(seq.m_curves.at(0), sampleTime);
                prop.asVec3()->y = SampleCurveValue(seq.m_curves.at(1), sampleTime);
                prop.asVec3()->z = SampleCurveValue(seq.m_curves.at(2), sampleTime);
                break;
            case inspector::PinDataType::Vec4:
                prop.asVec4()->x = SampleCurveValue(seq.m_curves.at(0), sampleTime);
                prop.asVec4()->y = SampleCurveValue(seq.m_curves.at(1), sampleTime);
                prop.asVec4()->z = SampleCurveValue(seq.m_curves.at(2), sampleTime);
                prop.asVec4()->w = SampleCurveValue(seq.m_curves.at(3), sampleTime);
                break;
            case inspector::PinDataType::Quat:
                *prop.asQuat() = SampleQuatForAnimation(seq, sampleTime);
                break;
            case inspector::PinDataType::Color:
            {
                auto* c = prop.asColor();
                c->r = static_cast<uint8_t>(
                    std::roundf(SampleCurveValue(seq.m_curves.at(0), sampleTime) * 255.f));
                c->g = static_cast<uint8_t>(
                    std::roundf(SampleCurveValue(seq.m_curves.at(1), sampleTime) * 255.f));
                c->b = static_cast<uint8_t>(
                    std::roundf(SampleCurveValue(seq.m_curves.at(2), sampleTime) * 255.f));
                c->a = static_cast<uint8_t>(
                    std::roundf(SampleCurveValue(seq.m_curves.at(3), sampleTime) * 255.f));
                break;
            }
            default:
                break;
        }
        return;
    }
}

inline void Record(const std::vector<inspector::ReflectedProperty>& props,
                   const std::string&                                fieldName,
                   int                                               recordingFrame,
                   Sequence&                                         seq)
{
    for (const auto& prop : props)
    {
        if (prop.name != fieldName) continue;

        const float rf = static_cast<float>(recordingFrame);
        auto ki = [&](int ci) { return seq.GetKeyframeIdx(ci, recordingFrame); };

        switch (prop.type)
        {
            case inspector::PinDataType::Float:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, *prop.asFloat()});
                break;
            case inspector::PinDataType::Int:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(),
                                            {rf, static_cast<float>(*prop.asInt())});
                break;
            case inspector::PinDataType::Bool:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(),
                                            {rf, *prop.asBool() ? 1.f : 0.f});
                break;
            case inspector::PinDataType::Vec2:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, prop.asVec2()->x});
                if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {rf, prop.asVec2()->y});
                break;
            case inspector::PinDataType::Vec3:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, prop.asVec3()->x});
                if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {rf, prop.asVec3()->y});
                if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {rf, prop.asVec3()->z});
                break;
            case inspector::PinDataType::Vec4:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, prop.asVec4()->x});
                if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {rf, prop.asVec4()->y});
                if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {rf, prop.asVec4()->z});
                if (ki(3)) seq.EditKeyframe(3, ki(3).value(), {rf, prop.asVec4()->w});
                break;
            case inspector::PinDataType::Quat:
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, prop.asQuat()->w});
                if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {rf, prop.asQuat()->x});
                if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {rf, prop.asQuat()->y});
                if (ki(3)) seq.EditKeyframe(3, ki(3).value(), {rf, prop.asQuat()->z});
                break;
            case inspector::PinDataType::Color:
            {
                auto* c = prop.asColor();
                if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {rf, c->r / 255.f});
                if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {rf, c->g / 255.f});
                if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {rf, c->b / 255.f});
                if (ki(3)) seq.EditKeyframe(3, ki(3).value(), {rf, c->a / 255.f});
                break;
            }
            default:
                break;
        }
        return;
    }
}

inline void Inspect(const std::vector<inspector::ReflectedProperty>& props,
                    const std::string&                                fieldName,
                    int                                               playerFrame,
                    Sequence&                                         seq,
                    bool                                              isPlaying)
{
    for (const auto& prop : props)
    {
        if (prop.name != fieldName) continue;

        const float pf  = static_cast<float>(playerFrame);
        auto ki  = [&](int ci) { return seq.GetKeyframeIdx(ci, playerFrame); };
        auto dis = [&](int ci) { return isPlaying || !ki(ci).has_value(); };

        switch (prop.type)
        {
            case inspector::PinDataType::Float:
            {
                const bool d = dis(0);
                if (d) ImGui::BeginDisabled();
                if (ImGui::InputFloat(fieldName.c_str(), prop.asFloat()) && !d)
                    seq.EditKeyframe(0, ki(0).value(), {pf, *prop.asFloat()});
                if (d) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Int:
            {
                const bool d = dis(0);
                if (d) ImGui::BeginDisabled();
                if (ImGui::InputInt(fieldName.c_str(), prop.asInt()) && !d)
                    seq.EditKeyframe(0, ki(0).value(),
                                     {pf, static_cast<float>(*prop.asInt())});
                if (d) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Bool:
            {
                const bool d = dis(0);
                if (d) ImGui::BeginDisabled();
                if (ImGui::Checkbox(fieldName.c_str(), prop.asBool()) && !d)
                    seq.EditKeyframe(0, ki(0).value(),
                                     {pf, *prop.asBool() ? 1.f : 0.f});
                if (d) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Vec2:
            {
                auto* v = prop.asVec2();
                auto lx = fieldName + ".x", ly = fieldName + ".y";
                const bool dx = dis(0), dy = dis(1);
                if (dx) ImGui::BeginDisabled();
                if (ImGui::InputFloat(lx.c_str(), &v->x) && !dx)
                    seq.EditKeyframe(0, ki(0).value(), {pf, v->x});
                if (dx) ImGui::EndDisabled();
                if (dy) ImGui::BeginDisabled();
                if (ImGui::InputFloat(ly.c_str(), &v->y) && !dy)
                    seq.EditKeyframe(1, ki(1).value(), {pf, v->y});
                if (dy) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Vec3:
            {
                auto* v  = prop.asVec3();
                const bool d = dis(0) || dis(1) || dis(2);
                if (d) ImGui::BeginDisabled();
                if (ImGui::InputFloat3(fieldName.c_str(), (float*)v) && !d)
                {
                    if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {pf, v->x});
                    if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {pf, v->y});
                    if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {pf, v->z});
                }
                if (d) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Vec4:
            {
                auto* v  = prop.asVec4();
                const bool d = dis(0) || dis(1) || dis(2) || dis(3);
                if (d) ImGui::BeginDisabled();
                if (ImGui::InputFloat4(fieldName.c_str(), (float*)v) && !d)
                {
                    if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {pf, v->x});
                    if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {pf, v->y});
                    if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {pf, v->z});
                    if (ki(3)) seq.EditKeyframe(3, ki(3).value(), {pf, v->w});
                }
                if (d) ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Quat:
            {
                ImGui::BeginDisabled();
                ImGui::DragFloat4(fieldName.c_str(), (float*)prop.asQuat());
                ImGui::EndDisabled();
                break;
            }
            case inspector::PinDataType::Color:
            {
                auto* col = prop.asColor();
                float rgba[4] = {col->r / 255.f, col->g / 255.f,
                                 col->b / 255.f, col->a / 255.f};
                const bool d = dis(0) || dis(1) || dis(2) || dis(3);
                if (d) ImGui::BeginDisabled();
                if (ImGui::ColorEdit4(fieldName.c_str(), rgba) && !d)
                {
                    col->r = static_cast<uint8_t>(rgba[0] * 255.f);
                    col->g = static_cast<uint8_t>(rgba[1] * 255.f);
                    col->b = static_cast<uint8_t>(rgba[2] * 255.f);
                    col->a = static_cast<uint8_t>(rgba[3] * 255.f);
                    if (ki(0)) seq.EditKeyframe(0, ki(0).value(), {pf, rgba[0]});
                    if (ki(1)) seq.EditKeyframe(1, ki(1).value(), {pf, rgba[1]});
                    if (ki(2)) seq.EditKeyframe(2, ki(2).value(), {pf, rgba[2]});
                    if (ki(3)) seq.EditKeyframe(3, ki(3).value(), {pf, rgba[3]});
                }
                if (d) ImGui::EndDisabled();
                break;
            }
            default:
                break;
        }
        return;
    }
}

}  // namespace reflection

// ─────────────────────────────────────────────────────────────────────────────
// RegisteredComponent — one entry per registered component type
// ─────────────────────────────────────────────────────────────────────────────

struct RegisteredComponent
{
    std::string                              m_struct_name{};
    std::vector<std::string>                 m_field_names{};
    std::vector<std::string>                 m_struct_field_names{};
    std::vector<inspector::PinDataType>      m_field_types{};

    // Fills a ComponentInspector from a live component pointer.
    std::function<void(void*, inspector::ComponentInspector&)> m_reflect;

    std::function<void(const entt::registry&, TimelineData&,
                       const ComponentData&, SequenceId&)>          m_add_sequence;
    std::function<void(entt::registry&, entt::entity, float,
                       Sequence&)>                                   m_sample;
    std::function<void(entt::registry&, entt::entity, int,
                       Sequence&, bool)>                             m_inspect;
    std::function<void(const entt::registry&, entt::entity, int,
                       Sequence&)>                                   m_record;
    std::function<bool(const entt::registry&, entt::entity)>        m_entity_has;

    bool HasStructFieldName(const std::string& sfn) const
    {
        return std::find(m_struct_field_names.begin(),
                         m_struct_field_names.end(), sfn) != m_struct_field_names.end();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Registry
// ─────────────────────────────────────────────────────────────────────────────

class Registry
{
public:
    // Register a component type.
    //
    // structName — stable string identifier embedded in serialized JSON.
    //   Providing an explicit name is strongly recommended for persistence.
    //   Defaults to typeid(T).name() which is stable within a build but
    //   compiler-specific and may include mangled characters.
    //
    // Prerequisite: specialize inspector::registerProperties<T> (from
    //   ComponentInspector.h) to describe the animatable fields.
    template <typename T>
    void RegisterComponent(const std::string& structName = typeid(T).name())
    {
        if (IsRegisteredOnce(structName)) return;

        // Gather field metadata from a default-constructed instance.
        T defaultComp{};
        inspector::ComponentInspector metaCi("_meta");
        inspector::registerProperties(defaultComp, metaCi);

        RegisteredComponent rc;
        rc.m_struct_name = structName;

        rc.m_reflect = [](void* ptr, inspector::ComponentInspector& ci)
        {
            inspector::registerProperties(*static_cast<T*>(ptr), ci);
        };

        for (const auto& prop : metaCi.getReflectedProperties())
        {
            rc.m_field_names.push_back(prop.name);
            rc.m_field_types.push_back(prop.type);
            rc.m_struct_field_names.push_back(
                helpers::MakeStructFieldName(structName, prop.name));
        }

        rc.m_entity_has = [](const entt::registry& r, entt::entity e)
        { return r.all_of<T>(e); };

        auto reflectFn = rc.m_reflect;

        rc.m_add_sequence = [reflectFn](const entt::registry& r,
                                        TimelineData& td,
                                        const ComponentData& cd,
                                        SequenceId& sid)
        {
            const auto entity = Timeline::FindEntity(cd, sid.GetEntityData().m_uid);
            if (entity == entt::null) return;
            inspector::ComponentInspector ci("_");
            // safe const_cast: we only read values to seed initial keyframes
            reflectFn(const_cast<void*>(
                static_cast<const void*>(&r.get<T>(entity))), ci);
            reflection::AddSequence(ci.getReflectedProperties(),
                                    sid.FieldName(), td, sid);
        };

        rc.m_sample = [reflectFn](entt::registry& r, entt::entity e,
                                   float t, Sequence& seq)
        {
            if (e == entt::null || !r.all_of<T>(e)) return;
            inspector::ComponentInspector ci("_");
            reflectFn(&r.get<T>(e), ci);
            reflection::Sample(ci.getReflectedProperties(),
                               seq.m_seq_id.FieldName(), t, seq);
        };

        rc.m_inspect = [reflectFn](entt::registry& r, entt::entity e,
                                    int frame, Sequence& seq, bool playing)
        {
            if (e == entt::null || !r.all_of<T>(e)) return;
            inspector::ComponentInspector ci("_");
            reflectFn(&r.get<T>(e), ci);
            reflection::Inspect(ci.getReflectedProperties(),
                                seq.m_seq_id.FieldName(), frame, seq, playing);
        };

        rc.m_record = [reflectFn](const entt::registry& r, entt::entity e,
                                   int frame, Sequence& seq)
        {
            if (e == entt::null || !r.all_of<T>(e)) return;
            inspector::ComponentInspector ci("_");
            reflectFn(const_cast<void*>(
                static_cast<const void*>(&r.get<T>(e))), ci);
            reflection::Record(ci.getReflectedProperties(),
                               seq.m_seq_id.FieldName(), frame, seq);
        };

        m_components.push_back(std::move(rc));
    }

    const std::vector<RegisteredComponent>& GetComponents() { return m_components; }

private:
    std::vector<RegisteredComponent> m_components;

    bool IsRegisteredOnce(const std::string& name) const
    {
        for (const auto& c : m_components)
            if (c.m_struct_name == name) return true;
        return false;
    }
};

inline Registry& GetRegistry()
{
    static Registry instance;
    return instance;
}

}  // namespace tanim::internal
