#include "core/Hierarchy.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "core/World.h"

// Parent -> children, in World order. NullEntity groups the roots.
using ChildrenMap = std::unordered_map<Entity, std::vector<Entity>>;

static ChildrenMap buildChildren(const World& world, const std::vector<Entity>& order) {
    ChildrenMap children;
    for (Entity e : order) children[parentOf(world, e)].push_back(e);
    return children;
}

// Depth-first walk: each entity followed by its branch.
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
    // Step limit: with fixHierarchy there are no cycles, but just in case it cannot hang.
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

    // e goes right before 'before' (or at the end), and then everything is ordered
    // like the tree: among siblings, the order of the list rules.
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

// e's parent does not exist, or e is part of a cycle (going up through the parents
// leads back to e). If e merely hangs from a cycle, no: cutting the cycle fixes it.
static bool hasBrokenParent(const World& world, Entity e) {
    const Parent* p = world.get<Parent>(e);
    if (!p) return false;
    if (!world.isAlive(p->entity)) return true;
    std::unordered_set<Entity> seen;
    for (Entity a = p->entity; a != NullEntity; a = parentOf(world, a)) {
        if (a == e) return true;
        if (!seen.insert(a).second) return false; // a cycle further up, without e
    }
    return false;
}

void fixHierarchy(World& world) {
    // One at a time: cutting a cycle at one entity repairs it for the others in the
    // cycle, which then no longer show up as broken.
    for (Entity e : world.entities())
        if (hasBrokenParent(world, e)) world.remove<Parent>(e);
}
