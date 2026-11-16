#pragma once
#include <cstdint>

// An entity is ONLY a number: it has no data and no behavior. What makes it a
// "player" or a "fruit" are the components the World associates with that number.
//
// Ids are NEVER reused within a World (they count up from 1), so storing an Entity
// and later asking world.isAlive(e) is safe: if the entity was destroyed, the
// answer is false and there is no dangling pointer.
// The id is also saved in the scene file, so it is stable across sessions.

using Entity = std::uint32_t;

constexpr Entity NullEntity = 0; // "no entity"
