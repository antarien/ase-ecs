#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        entity.hpp
 * @brief       The ECS vocabulary - entity handle, registry alias, view aliases, id helpers
 * @description Every name here is a thin alias over EnTT plus the one assertion that freezes the
 *              entity id width. It carries no class and no behaviour: whoever writes a system
 *              needs these names, and so does whoever uses the World facade.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    structure/datatype
 * @created     2026-08-30
 * @modified    2026-08-30
 * @version     1.0.0
 *
 * WHY THIS FILE EXISTS (split from system.hpp, 2026-08-30)
 *
 * system.hpp carried 625 lines and three purposes: this vocabulary, the System base class, and
 * the World facade. The band rule names that a missing separation, and the seam here is not a
 * matter of taste - it is what makes the other two SEPARABLE AT ALL. World needs Entity and
 * Registry; System needs them too. Leaving them in either file would force the other to include
 * it, and a mutual include is not a seam, it is a knot.
 *
 * WHY IT IS NOT CALLED types.hpp, WHICH IS WHERE A READER WOULD LOOK FIRST
 *
 * It was, for about ten minutes. The validator types a file by its NAME, and `types\.hpp$` wins
 * over the core-header catch-all: a file called types.hpp is the module's SSOT for CONSTANTS, and
 * its required header block says so ("All constants defined (no magic numbers in code)"). This
 * file defines no constant at all - it defines aliases. Filling that template in would have made
 * the gate green by writing a promise the file does not keep, and the next reader would look here
 * for values that live in schedule.hpp and app.hpp. The name follows the content instead.
 *
 * NOTHING MOVED IN SUBSTANCE. Every alias, the wire-contract assertion and both id helpers are
 * the ones system.hpp declared, character for character, with their reasoning blocks attached.
 *
 * THE INCLUDE COST IS UNCHANGED FOR EVERY EXISTING READER: system.hpp includes this file, so all
 * 1962 files that include system.hpp keep seeing every name they saw before. A reader who needs
 * only the vocabulary may include this file directly; nobody has to.
 *
 * CORE INFRASTRUCTURE COMPLIANCE
 *
 * [ ] NOT an ECS Component or System
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] No global mutable state (constexpr/const only)
 * [ ] No singletons or static mutable variables
 * [ ] Thread-safe by design (pure functions or explicit mutex)
 * [ ] All public functions documented with @brief, @param, @return
 * [ ] constexpr where possible (compile-time evaluation)
 * [ ] noexcept where possible (no-throw guarantee)
 * [ ] [[nodiscard]] on functions returning values
 * [ ] No magic numbers (use named constants)
 * [ ] No implicit conversions (use explicit constructors)
 * [ ] Header-only OR header+cpp pattern (not mixed)
 * [ ] Include guards via #pragma once
 * [ ] Namespace matches module: ase::{module}
 * [ ] No circular dependencies
 * [ ] No macros (except include guards) - use constexpr/templates
 * [ ] API stable (changes require version bump)
 */

#include <entt/entt.hpp>
#include <cstdint>

namespace ase::ecs {

/** Core Type Aliases */

/// Entity handle
using Entity = entt::entity;

// WIRE CONTRACT A6 (PLAN_ASE_COMPUTE_MOD_AXIS.md T1): the entity id width is FROZEN
// as a wire contract - u32 (20-bit index, 12-bit version). An ENTT_ID_TYPE override
// is forbidden: the only mismatch guard EnTT ships is MSVC-only (config.h detect_
// mismatch), so under Linux NOTHING else stops two binaries of different id width
// from being mixed on one wire. This assert makes the freeze a build error instead.
static_assert(sizeof(entt::entity) == sizeof(uint32_t),
              "ENTT_ID_TYPE must stay the uint32 default - the entity id width is a "
              "frozen wire contract (region_wire.hpp EntitySnap, mod-axis condition A6)");

/// Null entity constant
inline constexpr Entity NullEntity = entt::null;

/// Entity registry (the "world")
using Registry = entt::registry;

/// Component storage traits
template<typename Component>
using Storage = entt::storage<Component>;

/** View Types (for querying entities) */

/// View with specific components (read/write)
/**
 * VIEW TYPE ALIASES — numbered, because an alias template cannot be overloaded.
 *
 * A function can carry the same name at several arities; `using` cannot. Fixed arities
 * therefore force distinct names here, and that is a consequence of the required form, not a
 * rename to dodge a rule: the old `View<A, B>` spelling has **zero** uses in the tree
 * (measured 2026-08-22 with comments and string literals masked, against a positive control
 * that finds 33419 `registry.view<>` calls). No call site changes.
 *
 * ARITY FOUR matches the World::view() ladder in world.hpp so the two do not diverge. Whoever
 * needs a fifth adds one line here and one overload there.
 */
template<typename C0>
using View1 = entt::view<entt::get_t<C0>>;

template<typename C0, typename C1>
using View2 = entt::view<entt::get_t<C0, C1>>;

template<typename C0, typename C1, typename C2>
using View3 = entt::view<entt::get_t<C0, C1, C2>>;

template<typename C0, typename C1, typename C2, typename C3>
using View4 = entt::view<entt::get_t<C0, C1, C2, C3>>;

/** Entity ID Utilities */

/**
 * @brief Get numeric ID from entity
 * @param entity The entity handle to read
 * @return The entity's integral id, index and version packed as EnTT stores them
 */
[[nodiscard]] inline uint32_t entity_id(Entity entity) {
    return static_cast<uint32_t>(entt::to_integral(entity));
}

/**
 * @brief Get entity version
 * @param entity The entity handle to read
 * @return The version part of the handle, which distinguishes a reused index
 */
[[nodiscard]] inline uint32_t entity_version(Entity entity) {
    return entt::to_version(entity);
}

}  // namespace ase::ecs
