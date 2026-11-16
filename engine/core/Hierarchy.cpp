#include "core/Hierarchy.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "core/World.h"

// Padre -> hijos, en el orden del World. NullEntity agrupa a las raices.
using ChildrenMap = std::unordered_map<Entity, std::vector<Entity>>;

static ChildrenMap buildChildren(const World& world, const std::vector<Entity>& order) {
    ChildrenMap children;
    for (Entity e : order) children[parentOf(world, e)].push_back(e);
    return children;
}

// Recorrido en profundidad: cada entidad seguida de su rama.
static void appendBranch(const ChildrenMap& children, Entity e, std::vector<Entity>& out) {
    auto it = children.find(e);
    if (it == children.end()) return;
    for (Entity child : it->second) {
        out.push_back(child);
        appendBranch(children, child, out);
    }
}

Entity parentOf(const World& world, Entity e) {
    const Parent* p = world.get<Parent>(e);
    return p && world.isAlive(p->entity) ? p->entity : NullEntity;
}

std::vector<Entity> childrenOf(const World& world, Entity parent) {
    std::vector<Entity> out;
    for (Entity e : world.entities())
        if (parentOf(world, e) == parent) out.push_back(e);
    return out;
}

std::vector<Entity> descendantsOf(const World& world, Entity e) {
    std::vector<Entity> out;
    if (!world.isAlive(e)) return out;
    appendBranch(buildChildren(world, world.entities()), e, out);
    return out;
}

bool isAncestorOf(const World& world, Entity ancestor, Entity e) {
    if (ancestor == NullEntity) return false;
    // Tope de pasos: con fixHierarchy no hay ciclos, pero por las dudas no se cuelga.
    std::size_t steps = world.entities().size();
    for (Entity p = parentOf(world, e); p != NullEntity && steps-- > 0; p = parentOf(world, p))
        if (p == ancestor) return true;
    return false;
}

bool setParent(World& world, Entity child, Entity parent) {
    if (!world.isAlive(child)) return false;
    if (parent != NullEntity && (!world.isAlive(parent) || parent == child || isAncestorOf(world, child, parent)))
        return false;
    if (parent == NullEntity) world.remove<Parent>(child);
    else if (Parent* p = world.get<Parent>(child)) p->entity = parent;
    else world.add<Parent>(child).entity = parent;
    return true;
}

bool moveInHierarchy(World& world, Entity e, Entity parent, Entity before) {
    if (!setParent(world, e, parent)) return false;

    // e pasa a estar justo antes de 'before' (o al final), y despues todo se ordena
    // como el arbol: entre hermanos manda el orden de la lista.
    std::vector<Entity> order = world.entities();
    order.erase(std::remove(order.begin(), order.end(), e), order.end());
    auto at = before != NullEntity && before != e ? std::find(order.begin(), order.end(), before) : order.end();
    order.insert(at, e);

    std::vector<Entity> tree;
    tree.reserve(order.size());
    appendBranch(buildChildren(world, order), NullEntity, tree);
    world.setOrder(tree);
    return true;
}

void destroyWithChildren(World& world, Entity e) {
    for (Entity d : descendantsOf(world, e)) world.destroy(d);
    world.destroy(e);
}

// El padre de e no existe, o e es parte de un ciclo (subiendo por los padres se
// vuelve a e). Si e solo cuelga de un ciclo, no: lo arregla cortar el ciclo.
static bool hasBrokenParent(const World& world, Entity e) {
    const Parent* p = world.get<Parent>(e);
    if (!p) return false;
    if (!world.isAlive(p->entity)) return true;
    std::unordered_set<Entity> seen;
    for (Entity a = p->entity; a != NullEntity; a = parentOf(world, a)) {
        if (a == e) return true;
        if (!seen.insert(a).second) return false; // ciclo mas arriba, sin e
    }
    return false;
}

void fixHierarchy(World& world) {
    // De a una: cortar un ciclo en una entidad lo repara para las demas del ciclo,
    // que despues ya no dan roto.
    for (Entity e : world.entities())
        if (hasBrokenParent(world, e)) world.remove<Parent>(e);
}
