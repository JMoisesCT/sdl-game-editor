#pragma once
#include <vector>

#include "core/Entity.h"

class World;

// HIERARCHY: an entity can be a CHILD of another, to organize the scene into groups
// ("Level / Spikes", "HUD"...). It is an ORGANIZATIONAL hierarchy:
//   - each entity's Transform is still its position in the world (physics,
//     drawing and rules know nothing about parents);
//   - in the editor, moving the parent also moves its children, and deleting or
//     duplicating it deletes or duplicates its whole branch;
//   - while playing, children do NOT follow the parent: if a system destroys the
//     parent, the children are left on their own (to take them along, destroyWithChildren).
//
// In the scene file, a child stores its parent's id next to its own:
//     { "id": 7, "parent": 3, "components": { ... } }
// The order among siblings is the order of the entities in the World (and in the file).
//
// From code:
//     for (Entity child : childrenOf(world, group)) { ... }
//     Entity p = parentOf(world, e); // NullEntity if it has none

// Marks an entity as a child. Not registered in the reflection (like PrefabInstance):
// the serializer writes it separately ("parent": id) and the editor changes it by
// dragging in the Hierarchy. Change it with setParent / moveInHierarchy, not by hand:
// those functions prevent cycles.
struct Parent {
    Entity entity = NullEntity;
};

// The parent of e, or NullEntity if it has none (or if its parent no longer exists).
Entity parentOf(const World& world, Entity e);

// The direct children of 'parent', in World order. With NullEntity, the entities
// without a parent (the roots).
std::vector<Entity> childrenOf(const World& world, Entity parent);

// The whole branch under e (children, grandchildren...), in the order shown in the
// Hierarchy. Does not include e.
std::vector<Entity> descendantsOf(const World& world, Entity e);

// true if 'ancestor' is the parent of e, or its parent's parent, etc.
bool isAncestorOf(const World& world, Entity ancestor, Entity e);

// Makes 'child' a child of 'parent' (NullEntity = no parent). false, changing nothing,
// if it would create a cycle (an entity cannot hang from itself or from its branch).
bool setParent(World& world, Entity child, Entity parent);

// What the editor does when dragging in the Hierarchy: changes e's parent and puts it
// right before the sibling 'before' (NullEntity = the last of its siblings). Then it
// reorders the World like the tree (each parent followed by its branch), so the file
// is grouped the same way as the Hierarchy. false if it would create a cycle.
bool moveInHierarchy(World& world, Entity e, Entity parent, Entity before = NullEntity);

// Destroys (deferred, with World::destroy) e and its whole branch.
void destroyWithChildren(World& world, Entity e);

// Repairs what a hand-edited file may have broken: removes the parent from entities
// whose parent does not exist or that form a cycle. Called by deserializeWorld.
void fixHierarchy(World& world);
