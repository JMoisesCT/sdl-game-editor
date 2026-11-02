#pragma once
#include <map>
#include <string>

#include "core/Entity.h"
#include "core/SceneSerializer.h"

class ComponentRegistry;
class World;

// PREFABS: una entidad "molde" guardada en su propio archivo (.prefab.json), con sus
// componentes. Ocupan el lugar de las funciones setupX y del catalogo de objetos del
// v1: en vez de una funcion de C++ que arma la fruta, hay un archivo con los
// componentes de la fruta, que se edita en el editor.
//
// Una INSTANCIA es una entidad de la escena hecha a partir de un prefab. Queda
// ENLAZADA a el: en la escena solo se guardan los campos en que difiere (sus
// "overrides", como la posicion o la textura de OTRA fruta), y todo lo demas sale del
// prefab al cargar. Asi, cambiar el prefab (la caja de los pinchos, la vida del
// jugador...) cambia todas sus instancias de una vez. Ver SceneSerializer.h.
//
// En el editor: el panel Prefabs coloca instancias (clic o arrastrar a la vista), y
// el Inspector de una instancia ofrece Aplicar (sus cambios pasan al prefab), Revertir
// (vuelve a ser igual al prefab, salvo la posicion y el nombre) y Desvincular.
//
// Desde codigo (p. ej. un sistema que dispara balas):
//     Entity bala = prefabs.instantiate(world, "assets/prefabs/bullet.prefab.json");
//     world.get<Transform>(bala)->position = canon;

// Marca que una entidad es instancia de un prefab. No se registra en la reflexion: el
// serializador la escribe aparte ("prefab": ruta) y el Inspector la muestra arriba.
struct PrefabInstance {
    std::string path; // ruta del .prefab.json, relativa a la carpeta del ejecutable
};

// Los prefabs leidos, por ruta. Los lee una sola vez (hasta clear()) y los guarda en
// forma COMPLETA: todos los campos de cada componente, con los valores por defecto ya
// puestos. Asi comparar una instancia contra su prefab es comparar JSON campo a campo.
class PrefabLibrary {
public:
    explicit PrefabLibrary(const ComponentRegistry& registry) : registry(registry) {}

    // Los componentes del prefab (un objeto JSON "Nombre": {campos}), o nullptr si el
    // archivo no existe o no es un prefab (se avisa en el log una vez).
    const Json* find(const std::string& path);

    // Guarda esos componentes como el prefab de esa ruta (crea la carpeta si hace
    // falta) y actualiza lo leido. false si no se pudo escribir.
    bool save(const std::string& path, const Json& components);

    // Olvida lo leido: el proximo find vuelve a leer el disco.
    void clear() { cache.clear(); }

    // Crea una entidad nueva enlazada al prefab, con todos sus componentes. NullEntity
    // si el prefab no se puede leer. Como cualquier alta, no se llama dentro de un
    // each que recorra componentes que el prefab tenga.
    Entity instantiate(World& world, const std::string& path);

private:
    Json complete(const Json& components) const; // con todos los campos

    const ComponentRegistry& registry;
    std::map<std::string, Json> cache; // ruta -> componentes; null = no se pudo leer
};
