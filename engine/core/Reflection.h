#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
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

// Tipos de campo que el motor sabe guardar y editar. Para sumar uno: agregarlo aqui,
// en fieldTypeOf, en el guardado (SceneSerializer.cpp) y en el Inspector.
enum class FieldType { Bool, Int, Float, String, Vec2, Color, IntRect };

// Pista para el editor: como mostrar el campo (el tipo sigue siendo el mismo).
enum class FieldHint {
    None,
    TexturePath, // String: ruta a una imagen; el Inspector ofrece elegirla de assets/
};

struct FieldInfo {
    std::string name;     // clave en el JSON y etiqueta en el Inspector
    FieldType type = FieldType::Float;
    FieldHint hint = FieldHint::None;
    float min = 0.0f;     // si max > min, el editor limita el valor a [min, max]
    float max = 0.0f;
    float speed = 1.0f;   // cuanto cambia al arrastrar en el Inspector
    std::string tooltip;  // ayuda al pasar el raton (opcional)
    // Dado el componente (como void*), devuelve la direccion de este campo.
    std::function<void*(void*)> access;

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
    else static_assert(sizeof(M) == 0, "Tipo de campo no soportado por la reflexion");
}

// Ayudante del registro: cada metodo modifica el ultimo campo agregado.
template <class T>
class ComponentBuilder {
public:
    explicit ComponentBuilder(ComponentInfo& info) : info(info) {}

    template <class M>
    ComponentBuilder& field(const char* name, M T::*member) {
        FieldInfo f;
        f.name = name;
        f.type = fieldTypeOf<M>();
        f.speed = std::is_same_v<M, float> ? 0.1f : 1.0f;
        f.access = [member](void* c) -> void* { return &(static_cast<T*>(c)->*member); };
        info.fields.push_back(std::move(f));
        return *this;
    }

    ComponentBuilder& range(float min, float max) { last().min = min; last().max = max; return *this; }
    ComponentBuilder& speed(float s) { last().speed = s; return *this; }
    ComponentBuilder& tooltip(const char* text) { last().tooltip = text; return *this; }
    ComponentBuilder& texture() { last().hint = FieldHint::TexturePath; return *this; }
    // El editor no ofrece quitar este componente (p. ej. EntityInfo).
    ComponentBuilder& notRemovable() { info.removable = false; return *this; }

private:
    FieldInfo& last() { return info.fields.back(); }
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
