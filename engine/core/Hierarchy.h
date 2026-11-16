#pragma once
#include <vector>

#include "core/Entity.h"

class World;

// JERARQUIA: una entidad puede ser HIJA de otra, para ordenar la escena en grupos
// ("Nivel / Pinchos", "HUD"...). Es una jerarquia de ORGANIZACION:
//   - el Transform de cada entidad sigue siendo su posicion en el mundo (la fisica,
//     el dibujo y las reglas no saben nada de padres);
//   - en el editor, mover al padre mueve tambien a sus hijos, y borrarlo o duplicarlo
//     borra o duplica a toda su rama;
//   - jugando, los hijos NO siguen al padre: si un sistema destruye al padre, los
//     hijos quedan sueltos (para llevarselos, destroyWithChildren).
//
// En el archivo de escena, una hija lleva el id de su padre junto al suyo:
//     { "id": 7, "parent": 3, "components": { ... } }
// El orden entre hermanos es el orden de las entidades en el World (y en el archivo).
//
// Desde codigo:
//     for (Entity hijo : childrenOf(world, grupo)) { ... }
//     Entity p = parentOf(world, e); // NullEntity si no tiene

// Marca a una entidad como hija. No se registra en la reflexion (como PrefabInstance):
// el serializador la escribe aparte ("parent": id) y el editor la cambia arrastrando
// en la Jerarquia. Se cambia con setParent / moveInHierarchy, no a mano: esas
// funciones impiden los ciclos.
struct Parent {
    Entity entity = NullEntity;
};

// El padre de e, o NullEntity si no tiene (o si su padre ya no existe).
Entity parentOf(const World& world, Entity e);

// Los hijos directos de 'parent', en el orden del World. Con NullEntity, las entidades
// sin padre (las raices).
std::vector<Entity> childrenOf(const World& world, Entity parent);

// Toda la rama bajo e (hijos, nietos...), en el orden en que se ve en la Jerarquia.
// No incluye a e.
std::vector<Entity> descendantsOf(const World& world, Entity e);

// true si 'ancestor' es el padre de e, o el padre de su padre, etc.
bool isAncestorOf(const World& world, Entity ancestor, Entity e);

// Hace a 'child' hija de 'parent' (NullEntity = sin padre). false, sin cambiar nada,
// si formaria un ciclo (una entidad no puede colgar de si misma ni de su rama).
bool setParent(World& world, Entity child, Entity parent);

// Lo que hace el editor al arrastrar en la Jerarquia: cambia el padre de e y la deja
// justo antes del hermano 'before' (NullEntity = la ultima de sus hermanas). Despues
// reordena el World como el arbol (cada padre seguido de su rama), asi el archivo
// queda agrupado igual que la Jerarquia. false si formaria un ciclo.
bool moveInHierarchy(World& world, Entity e, Entity parent, Entity before = NullEntity);

// Destruye (diferido, con World::destroy) a e y a toda su rama.
void destroyWithChildren(World& world, Entity e);

// Repara lo que un archivo editado a mano pudo dejar mal: quita el padre a las
// entidades cuyo padre no existe o que forman un ciclo. La llama deserializeWorld.
void fixHierarchy(World& world);
