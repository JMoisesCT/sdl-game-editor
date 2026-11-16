#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/Math.h"
#include "core/World.h"

// Minimal reflection: each component DECLARES its fields once, and from that list
// the engine knows how to, with no extra code per component:
//   - save and load it as JSON (SceneSerializer),
//   - show and edit it in the editor's Inspector,
//   - add or remove it from the editor ("Add component").
//
// Registration (once, at startup; see CoreComponents.cpp):
//     registry.add<Transform>("Transform")
//         .field("position", &Transform::position)
//         .field("rotation", &Transform::rotation).range(-360, 360)
//         .field("scale",    &Transform::scale);
//
// A field that is NOT registered is neither saved nor shown in the editor: it is
// runtime state (an internal timer, something a system computes...).
//
// LISTS OF STRUCTS: a std::vector<E> field, with E a data struct (the clips of a
// SpriteAnimator), is declared with list(), and E's fields are described just like
// a component's:
//     registry.add<SpriteAnimator>("SpriteAnimator")
//         .list("clips", &SpriteAnimator::clips, [](FieldsBuilder<AnimationClip>& clip) {
//             clip.field("name", &AnimationClip::name)
//                 .field("fps", &AnimationClip::fps);
//         });

// Field types the engine can save and edit. To add one: add it here, in
// fieldTypeOf, in saving (SceneSerializer.cpp) and in the Inspector.
//   IntList = std::vector<int> (e.g. the solid tiles of a Tilemap).
//   IntGrid = grid of integers (the cells of a Tilemap; see Math.h).
//   Entity  = reference to ANOTHER entity in the scene (a member of type Entity). It
//             is saved as its id (0 = none) and the Inspector lets you pick it from a
//             list or drop it by dragging it from the Hierarchy. It may point to an
//             entity that no longer exists: check with world.isAlive.
//   List    = std::vector<E> of structs (declared with list(), not with field()).
enum class FieldType { Bool, Int, Float, String, Vec2, Color, IntRect, IntList, IntGrid, Entity, List };

// Hint for the editor: how to show the field (the type stays the same).
enum class FieldHint {
    None,
    TexturePath, // String: path to an image; the Inspector lets you pick it from assets/
    AudioPath,   // String: path to a sound (wav, ogg, opus); picked from assets/ and playable
    Choice,      // String: the Inspector offers a list of values (FieldInfo::choices)
};

// Values a Choice field offers. It receives the entity being edited, so the list
// can depend on it (the clips of ITS SpriteAnimator). A "" in the list is shown
// as "(none)".
using ChoiceProvider = std::function<std::vector<std::string>(const World&, Entity)>;

struct FieldInfo {
    std::string name;     // key in the JSON and label in the Inspector
    FieldType type = FieldType::Float;
    FieldHint hint = FieldHint::None;
    float min = 0.0f;     // if max > min, the editor clamps the value to [min, max]
    float max = 0.0f;
    float speed = 1.0f;   // how much it changes when dragging in the Inspector
    std::string tooltip;  // help shown on hover (optional)
    ChoiceProvider choices; // only with the Choice hint
    // Given the component (as void*), returns the address of this field.
    std::function<void*(void*)> access;

    // --- Only for List: the fields of each element and how to operate the vector ---
    std::vector<FieldInfo> elementFields;
    std::function<std::size_t(const void*)> listSize;
    std::function<void(void*, std::size_t)> listResize; // the new one starts with its default values
    std::function<void*(void*, std::size_t)> listElement;
    std::function<void(void*, std::size_t)> listErase;
    std::function<void(void*, std::size_t, std::size_t)> listSwap;
    std::function<void(void*, std::size_t)> listDuplicate; // the copy goes right after the original

    void* in(void* component) const { return access(component); }
    const void* in(const void* component) const { return access(const_cast<void*>(component)); }
};

// Description of a registered component type.
struct ComponentInfo {
    std::string name;            // name in the JSON and in the editor ("Transform")
    std::size_t typeId = 0;      // componentTypeId<T>()
    std::vector<FieldInfo> fields;
    bool removable = true;       // false: the editor does not offer to remove it
    std::function<void*(World&, Entity)> addDefault; // adds T{} and returns its address

    bool has(const World& w, Entity e) const;
    void* get(World& w, Entity e) const;            // nullptr if it does not have it
    const void* get(const World& w, Entity e) const;
    void* add(World& w, Entity e) const { return addDefault(w, e); }
    void remove(World& w, Entity e) const;
};

// Maps the member's C++ type to its FieldType. An unsupported type does not compile.
template <class M>
constexpr FieldType fieldTypeOf() {
    if constexpr (std::is_same_v<M, bool>) return FieldType::Bool;
    else if constexpr (std::is_same_v<M, int>) return FieldType::Int;
    else if constexpr (std::is_same_v<M, float>) return FieldType::Float;
    else if constexpr (std::is_same_v<M, std::string>) return FieldType::String;
    else if constexpr (std::is_same_v<M, Vec2>) return FieldType::Vec2;
    else if constexpr (std::is_same_v<M, Color>) return FieldType::Color;
    else if constexpr (std::is_same_v<M, IntRect>) return FieldType::IntRect;
    else if constexpr (std::is_same_v<M, std::vector<int>>) return FieldType::IntList;
    else if constexpr (std::is_same_v<M, IntGrid>) return FieldType::IntGrid;
    // Entity is a std::uint32_t: any member of that type is a reference.
    else if constexpr (std::is_same_v<M, Entity>) return FieldType::Entity;
    else static_assert(sizeof(M) == 0, "Unsupported field type (a list of structs goes with list())");
}

// Registration helper: declares the fields of a struct T (a component or a list
// element). Each method modifies the last field added.
template <class T>
class FieldsBuilder {
public:
    explicit FieldsBuilder(std::vector<FieldInfo>& fields) : fields(fields) {}

    template <class M>
    FieldsBuilder& field(const char* name, M T::*member) {
        FieldInfo f;
        f.name = name;
        f.type = fieldTypeOf<M>();
        f.speed = std::is_same_v<M, float> ? 0.1f : 1.0f;
        f.access = [member](void* c) -> void* { return &(static_cast<T*>(c)->*member); };
        fields.push_back(std::move(f));
        return *this;
    }

    // List of structs E. 'describe' receives a FieldsBuilder<E> to declare its fields.
    template <class E, class Describe>
    FieldsBuilder& list(const char* name, std::vector<E> T::*member, Describe describe) {
        static_assert(std::is_default_constructible_v<E>, "A list element needs a default constructor");
        using V = std::vector<E>;
        FieldInfo f;
        f.name = name;
        f.type = FieldType::List;
        f.access = [member](void* c) -> void* { return &(static_cast<T*>(c)->*member); };
        f.listSize = [](const void* v) { return static_cast<const V*>(v)->size(); };
        f.listResize = [](void* v, std::size_t n) { static_cast<V*>(v)->resize(n); };
        f.listElement = [](void* v, std::size_t i) -> void* { return &(*static_cast<V*>(v))[i]; };
        f.listErase = [](void* v, std::size_t i) {
            V& list = *static_cast<V*>(v);
            if (i < list.size()) list.erase(list.begin() + (std::ptrdiff_t)i);
        };
        f.listSwap = [](void* v, std::size_t i, std::size_t j) {
            V& list = *static_cast<V*>(v);
            if (i < list.size() && j < list.size()) std::swap(list[i], list[j]);
        };
        f.listDuplicate = [](void* v, std::size_t i) {
            V& list = *static_cast<V*>(v);
            if (i >= list.size()) return;
            E copy = list[i];
            list.insert(list.begin() + (std::ptrdiff_t)i + 1, std::move(copy));
        };
        FieldsBuilder<E> elements(f.elementFields);
        describe(elements);
        fields.push_back(std::move(f));
        return *this;
    }

    FieldsBuilder& range(float min, float max) { last().min = min; last().max = max; return *this; }
    FieldsBuilder& speed(float s) { last().speed = s; return *this; }
    FieldsBuilder& tooltip(const char* text) { last().tooltip = text; return *this; }
    FieldsBuilder& texture() { last().hint = FieldHint::TexturePath; return *this; }
    FieldsBuilder& audio() { last().hint = FieldHint::AudioPath; return *this; }
    // A String with values to choose from (the Inspector shows a dropdown).
    FieldsBuilder& choices(ChoiceProvider provider) {
        last().hint = FieldHint::Choice;
        last().choices = std::move(provider);
        return *this;
    }
    FieldsBuilder& choices(std::vector<std::string> fixed) {
        return choices([fixed](const World&, Entity) { return fixed; });
    }

private:
    FieldInfo& last() { return fields.back(); }
    std::vector<FieldInfo>& fields;
};

// The component version: the same, plus the options of the whole component.
template <class T>
class ComponentBuilder : public FieldsBuilder<T> {
public:
    explicit ComponentBuilder(ComponentInfo& info) : FieldsBuilder<T>(info.fields), info(info) {}

    // The editor does not offer to remove this component (e.g. EntityInfo). Goes BEFORE
    // the fields: field() returns a FieldsBuilder, which does not have it.
    ComponentBuilder& notRemovable() { info.removable = false; return *this; }

private:
    ComponentInfo& info;
};

class ComponentRegistry {
public:
    // Registers type T with that name. The registration order is the order in which the
    // Inspector shows the components and in which they are saved to the file.
    template <class T>
    ComponentBuilder<T> add(const std::string& name) {
        static_assert(std::is_default_constructible_v<T>,
                      "A component needs a default constructor (it is created on load)");
        auto info = std::make_unique<ComponentInfo>();
        info->name = name;
        info->typeId = componentTypeId<T>();
        info->addDefault = [](World& w, Entity e) -> void* { return &w.add<T>(e); };
        ComponentInfo& ref = *info;
        infos.push_back(std::move(info));
        return ComponentBuilder<T>(ref);
    }

    const ComponentInfo* find(const std::string& name) const;
    const ComponentInfo* findByTypeId(std::size_t typeId) const;
    template <class T>
    const ComponentInfo* find() const { return findByTypeId(componentTypeId<T>()); }

    // Every registered component, in registration order.
    const std::vector<std::unique_ptr<ComponentInfo>>& all() const { return infos; }

private:
    // unique_ptr: the ComponentBuilder keeps a reference to the info, which must not
    // move when the vector grows.
    std::vector<std::unique_ptr<ComponentInfo>> infos;
};

// Calls fn with every Entity field of e's registered components (including those
// of list elements). fn may change the value: that is how the references of a copy
// are remapped (duplicating a branch) or cleared (when saving a prefab).
void forEachEntityField(const ComponentRegistry& registry, World& world, Entity e,
                        const std::function<void(Entity&)>& fn);
