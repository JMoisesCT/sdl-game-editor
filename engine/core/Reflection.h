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

// Reflexion minima: cada componente DECLARA sus campos una vez, y con esa lista el
// motor sabe, sin codigo extra por componente:
//   - guardarlo y cargarlo en JSON (SceneSerializer),
//   - mostrarlo y editarlo en el Inspector del editor,
//   - agregarlo o quitarlo desde el editor ("Agregar componente").
//
// Registro (una vez, al arrancar; ver CoreComponents.cpp):
//     registry.add<Transform>("Transform")
//         .field("position", &Transform::position)
//         .field("rotation", &Transform::rotation).range(-360, 360)
//         .field("scale",    &Transform::scale);
//
// Un campo que NO se registra no se guarda ni se ve en el editor: es estado de
// ejecucion (un temporizador interno, algo que calcula un sistema...).
//
// LISTAS DE STRUCTS: un campo std::vector<E>, con E un struct de datos (los clips de
// un SpriteAnimator), se declara con list(), y los campos de E se describen igual que
// los de un componente:
//     registry.add<SpriteAnimator>("SpriteAnimator")
//         .list("clips", &SpriteAnimator::clips, [](FieldsBuilder<AnimationClip>& clip) {
//             clip.field("name", &AnimationClip::name)
//                 .field("fps", &AnimationClip::fps);
//         });

// Tipos de campo que el motor sabe guardar y editar. Para sumar uno: agregarlo aqui,
// en fieldTypeOf, en el guardado (SceneSerializer.cpp) y en el Inspector.
//   IntList = std::vector<int> (p. ej. los tiles solidos de un Tilemap).
//   IntGrid = grilla de enteros (las celdas de un Tilemap; ver Math.h).
//   List    = std::vector<E> de structs (se declara con list(), no con field()).
enum class FieldType { Bool, Int, Float, String, Vec2, Color, IntRect, IntList, IntGrid, List };

// Pista para el editor: como mostrar el campo (el tipo sigue siendo el mismo).
enum class FieldHint {
    None,
    TexturePath, // String: ruta a una imagen; el Inspector ofrece elegirla de assets/
    Choice,      // String: el Inspector ofrece una lista de valores (FieldInfo::choices)
};

// Valores que ofrece un campo Choice. Recibe la entidad que se esta editando: asi la
// lista puede depender de ella (los clips de SU SpriteAnimator). Un "" en la lista se
// muestra como "(ninguno)".
using ChoiceProvider = std::function<std::vector<std::string>(const World&, Entity)>;

struct FieldInfo {
    std::string name;     // clave en el JSON y etiqueta en el Inspector
    FieldType type = FieldType::Float;
    FieldHint hint = FieldHint::None;
    float min = 0.0f;     // si max > min, el editor limita el valor a [min, max]
    float max = 0.0f;
    float speed = 1.0f;   // cuanto cambia al arrastrar en el Inspector
    std::string tooltip;  // ayuda al pasar el raton (opcional)
    ChoiceProvider choices; // solo con hint Choice
    // Dado el componente (como void*), devuelve la direccion de este campo.
    std::function<void*(void*)> access;

    // --- Solo para List: los campos de cada elemento y como operar el vector ---
    std::vector<FieldInfo> elementFields;
    std::function<std::size_t(const void*)> listSize;
    std::function<void(void*, std::size_t)> listResize; // lo nuevo nace con sus valores por defecto
    std::function<void*(void*, std::size_t)> listElement;
    std::function<void(void*, std::size_t)> listErase;
    std::function<void(void*, std::size_t, std::size_t)> listSwap;
    std::function<void(void*, std::size_t)> listDuplicate; // la copia queda detras del original

    void* in(void* component) const { return access(component); }
    const void* in(const void* component) const { return access(const_cast<void*>(component)); }
};

// Descripcion de un tipo de componente registrado.
struct ComponentInfo {
    std::string name;            // nombre en el JSON y en el editor ("Transform")
    std::size_t typeId = 0;      // componentTypeId<T>()
    std::vector<FieldInfo> fields;
    bool removable = true;       // false: el editor no ofrece quitarlo
    std::function<void*(World&, Entity)> addDefault; // agrega T{} y devuelve su direccion

    bool has(const World& w, Entity e) const;
    void* get(World& w, Entity e) const;            // nullptr si no lo tiene
    const void* get(const World& w, Entity e) const;
    void* add(World& w, Entity e) const { return addDefault(w, e); }
    void remove(World& w, Entity e) const;
};

// Traduce el tipo C++ del miembro al FieldType. Un tipo no soportado no compila.
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
    else static_assert(sizeof(M) == 0, "Tipo de campo no soportado (una lista de structs va con list())");
}

// Ayudante del registro: declara los campos de un struct T (un componente o el
// elemento de una lista). Cada metodo modifica el ultimo campo agregado.
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

    // Lista de structs E. 'describe' recibe un FieldsBuilder<E> para declarar sus campos.
    template <class E, class Describe>
    FieldsBuilder& list(const char* name, std::vector<E> T::*member, Describe describe) {
        static_assert(std::is_default_constructible_v<E>, "Un elemento de lista necesita constructor por defecto");
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
    // Un String con valores para elegir (el Inspector muestra una lista desplegable).
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

// El de un componente: lo mismo, mas las opciones del componente entero.
template <class T>
class ComponentBuilder : public FieldsBuilder<T> {
public:
    explicit ComponentBuilder(ComponentInfo& info) : FieldsBuilder<T>(info.fields), info(info) {}

    // El editor no ofrece quitar este componente (p. ej. EntityInfo). Va ANTES de los
    // campos: field() devuelve un FieldsBuilder, que no lo tiene.
    ComponentBuilder& notRemovable() { info.removable = false; return *this; }

private:
    ComponentInfo& info;
};

class ComponentRegistry {
public:
    // Registra el tipo T con ese nombre. El orden de registro es el orden en que el
    // Inspector muestra los componentes y en que se guardan en el archivo.
    template <class T>
    ComponentBuilder<T> add(const std::string& name) {
        static_assert(std::is_default_constructible_v<T>,
                      "Un componente necesita constructor por defecto (se crea al cargar)");
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

    // Todos los registrados, en orden de registro.
    const std::vector<std::unique_ptr<ComponentInfo>>& all() const { return infos; }

private:
    // unique_ptr: el ComponentBuilder guarda una referencia a la info, que no debe
    // moverse cuando el vector crece.
    std::vector<std::unique_ptr<ComponentInfo>> infos;
};
