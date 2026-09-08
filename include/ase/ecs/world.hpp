#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        world.hpp
 * @brief       The World facade - a typed, fixed-arity front for one EnTT registry
 * @description World owns a Registry and exposes entity creation, component access, views and
 *              signals through fixed-arity overloads instead of parameter packs. It runs nothing:
 *              schedule-based execution lives in App (app.cpp).
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
 * system.hpp carried 625 lines and three purposes: the ECS vocabulary, the System base class and
 * this facade. A file in the unsplit band is evidence that the separation is missing, and here it
 * was plain to see: a system author implements System and never touches World, while a caller who
 * builds a world never derives from System. Two audiences, two files.
 *
 * NOTHING WAS SHORTENED. Every member, every arity and every reasoning block moved character for
 * character, including the two tombstones below - the deleted system management and the arity
 * measurements. They document decisions about WORLD and belong where World is.
 *
 * NO EXISTING READER HAS TO CHANGE. system.hpp includes this file, so all 1962 files that include
 * system.hpp keep seeing World exactly as before. That is the whole reason the cut is safe at
 * this layer: the outward interface is unchanged, only the inner structure moved.
 *
 * WHY THE VOCABULARY IS A THIRD FILE and not kept here: World needs Entity and Registry, and so
 * does System. Leaving the aliases in either one would force the other to include it, and two
 * headers that include each other are not a seam. entity.hpp carries them for both - and it is
 * NOT called types.hpp on purpose, because that name makes a file its module's SSOT for
 * constants, and the vocabulary defines none. Its own head records that decision.
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

#include <ase/ecs/entity.hpp>

#include <type_traits>
// <algorithm> and <memory> WENT WITH World's system management: std::sort lived in
// sort_systems(), std::unique_ptr and std::make_unique in the systems_ container. Nothing else
// in this file uses either.
//
// MEASURED BEFORE REMOVING THEM, because this header is reached by 1962 files through system.hpp
// and whoever uses a symbol WITHOUT its own include gets it from here for free - a coupling no
// gate can see:
//     <memory>      0 files at risk (2 bring it themselves)
//     <algorithm>   0 files at risk
// The first run of that measurement said FIVE files were at risk for <memory>, and all five
// were checklist lines reading "[ ] NO std::shared_ptr in components" - the counter had read
// COMMENTS as usage, which is exactly the blind spot this session reported in the validator
// the same afternoon. The number above is from the corrected run, with comments masked.

namespace ase::ecs {

/** World (Registry + Systems) */

/**
 * The World contains all entities and runs systems
 *
 * This is the central object that owns the ECS registry and manages
 * system execution order.
 *
 * WHERE THE SCHEDULE-BASED EXECUTION LIVES - and why there is no world.cpp:
 *
 *   World implementation is now minimal - all schedule-based execution is
 *   handled by App (app.cpp). World only provides registry access and basic
 *   system management for direct use.
 *
 * That sentence stood in src/system.cpp, a translation unit of nine lines that
 * defined no symbol at all and carried nothing but this explanation. The file
 * was deleted on 2026-08-20 under Rule 15; the sentence travelled to system.hpp
 * then and here with World on 2026-08-30, because it is the answer to the
 * question the empty file used to raise. Anyone looking for the tick loop finds
 * it in app.cpp, not below this header.
 */
class World {
public:
    World() = default;
    ~World() = default;

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    /** Entity Management */

    /// Create a new entity
    [[nodiscard]] Entity create() {
        return registry_.create();
    }

    /// Create entity with hint (for predictable IDs)
    [[nodiscard]] Entity create(Entity hint) {
        return registry_.create(hint);
    }

    /// Destroy an entity
    void destroy(Entity entity) {
        registry_.destroy(entity);
    }

    /// Check if entity is valid
    [[nodiscard]] bool valid(Entity entity) const {
        return registry_.valid(entity);
    }

    /// Get entity count
    [[nodiscard]] size_t size() const {
        return registry_.storage<Entity>()->size();
    }

    /** Component Management */

    /**
     * ADD COMPONENT — fixed constructor arities instead of a parameter pack.
     *
     * ARITY FOUR: GEMESSEN 2026-08-22, all 25 world.emplace<> call sites in the tree pass at
     * most THREE constructor arguments (three floats for a position). GESETZT is four, one
     * spare. A fifth is one overload written after this pattern, and the COMPILER names the
     * call site that needs it — a missing overload cannot fail silently at run time.
     *
     * The empty-component branch stays in every arity: EnTT returns void for an empty type,
     * and `return registry_.emplace<T>(...)` on such a type does not compile. Dropping the
     * `if constexpr` would break every tag emplace in the tree.
     */
    template<typename Component>
    decltype(auto) emplace(Entity entity) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity);
        } else {
            return registry_.emplace<Component>(entity);
        }
    }

    template<typename Component, typename A0>
    decltype(auto) emplace(Entity entity, A0&& a0) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity, std::forward<A0>(a0));
        } else {
            return registry_.emplace<Component>(entity, std::forward<A0>(a0));
        }
    }

    template<typename Component, typename A0, typename A1>
    decltype(auto) emplace(Entity entity, A0&& a0, A1&& a1) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity, std::forward<A0>(a0), std::forward<A1>(a1));
        } else {
            return registry_.emplace<Component>(entity, std::forward<A0>(a0),
                                                std::forward<A1>(a1));
        }
    }

    template<typename Component, typename A0, typename A1, typename A2>
    decltype(auto) emplace(Entity entity, A0&& a0, A1&& a1, A2&& a2) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity, std::forward<A0>(a0), std::forward<A1>(a1),
                                         std::forward<A2>(a2));
        } else {
            return registry_.emplace<Component>(entity, std::forward<A0>(a0),
                                                std::forward<A1>(a1), std::forward<A2>(a2));
        }
    }

    template<typename Component, typename A0, typename A1, typename A2, typename A3>
    decltype(auto) emplace(Entity entity, A0&& a0, A1&& a1, A2&& a2, A3&& a3) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity, std::forward<A0>(a0), std::forward<A1>(a1),
                                         std::forward<A2>(a2), std::forward<A3>(a3));
        } else {
            return registry_.emplace<Component>(entity, std::forward<A0>(a0),
                                                std::forward<A1>(a1), std::forward<A2>(a2),
                                                std::forward<A3>(a3));
        }
    }

    /// Add or replace component — same arity ladder, same measurement
    template<typename Component>
    Component& emplace_or_replace(Entity entity) {
        return registry_.emplace_or_replace<Component>(entity);
    }

    template<typename Component, typename A0>
    Component& emplace_or_replace(Entity entity, A0&& a0) {
        return registry_.emplace_or_replace<Component>(entity, std::forward<A0>(a0));
    }

    template<typename Component, typename A0, typename A1>
    Component& emplace_or_replace(Entity entity, A0&& a0, A1&& a1) {
        return registry_.emplace_or_replace<Component>(entity, std::forward<A0>(a0),
                                                       std::forward<A1>(a1));
    }

    template<typename Component, typename A0, typename A1, typename A2>
    Component& emplace_or_replace(Entity entity, A0&& a0, A1&& a1, A2&& a2) {
        return registry_.emplace_or_replace<Component>(entity, std::forward<A0>(a0),
                                                       std::forward<A1>(a1),
                                                       std::forward<A2>(a2));
    }

    template<typename Component, typename A0, typename A1, typename A2, typename A3>
    Component& emplace_or_replace(Entity entity, A0&& a0, A1&& a1, A2&& a2, A3&& a3) {
        return registry_.emplace_or_replace<Component>(entity, std::forward<A0>(a0),
                                                       std::forward<A1>(a1),
                                                       std::forward<A2>(a2),
                                                       std::forward<A3>(a3));
    }

    /// Get component (throws if missing)
    template<typename Component>
    [[nodiscard]] Component& get(Entity entity) {
        return registry_.get<Component>(entity);
    }

    template<typename Component>
    [[nodiscard]] const Component& get(Entity entity) const {
        return registry_.get<Component>(entity);
    }

    /// Try get component (returns nullptr if missing)
    template<typename Component>
    [[nodiscard]] Component* try_get(Entity entity) {
        return registry_.try_get<Component>(entity);
    }

    template<typename Component>
    [[nodiscard]] const Component* try_get(Entity entity) const {
        return registry_.try_get<Component>(entity);
    }

    /// Check if entity has component
    template<typename Component>
    [[nodiscard]] bool has(Entity entity) const {
        return registry_.all_of<Component>(entity);
    }

    /**
     * CHECK ALL COMPONENTS — fixed arities. The one-type case is `has()` above, so this ladder
     * starts at two. GEMESSEN 2026-08-22: has_all appears in exactly ONE file tree-wide, and
     * that file was system.hpp, which then held this class — no caller exists. GESETZT is four,
     * matching the view ladder below, so the two do not diverge for a reader who uses them
     * together.
     */
    template<typename C0, typename C1>
    [[nodiscard]] bool has_all(Entity entity) const {
        return registry_.all_of<C0, C1>(entity);
    }

    template<typename C0, typename C1, typename C2>
    [[nodiscard]] bool has_all(Entity entity) const {
        return registry_.all_of<C0, C1, C2>(entity);
    }

    template<typename C0, typename C1, typename C2, typename C3>
    [[nodiscard]] bool has_all(Entity entity) const {
        return registry_.all_of<C0, C1, C2, C3>(entity);
    }

    /// Remove component from entity
    template<typename Component>
    void remove(Entity entity) {
        registry_.remove<Component>(entity);
    }

    /** Views (Queries) */

    /**
     * VIEWS — fixed arities instead of a parameter pack.
     *
     * ARITY FOUR: GEMESSEN 2026-08-22 over the whole tree. This facade carries SEVEN call
     * sites (four in test_ecs.cpp, one in kernel_test.cpp, one in README.md, one in the usage
     * example in system.hpp), and their **highest arity is two**. The 33419 calls that reach
     * `registry.view<>()` directly do NOT come through here — they go to EnTT, whose pack is not
     * this module's to change; measuring them together is what first suggested a ladder up to
     * nineteen for seven call sites.
     * GESETZT is four, which covers 97.3 % of the tree's view arities should this facade ever
     * be used more widely. A fifth is one overload plus one const overload.
     */
    template<typename C0>
    [[nodiscard]] auto view() {
        return registry_.view<C0>();
    }

    template<typename C0, typename C1>
    [[nodiscard]] auto view() {
        return registry_.view<C0, C1>();
    }

    template<typename C0, typename C1, typename C2>
    [[nodiscard]] auto view() {
        return registry_.view<C0, C1, C2>();
    }

    template<typename C0, typename C1, typename C2, typename C3>
    [[nodiscard]] auto view() {
        return registry_.view<C0, C1, C2, C3>();
    }

    template<typename C0>
    [[nodiscard]] auto view() const {
        return registry_.view<C0>();
    }

    template<typename C0, typename C1>
    [[nodiscard]] auto view() const {
        return registry_.view<C0, C1>();
    }

    template<typename C0, typename C1, typename C2>
    [[nodiscard]] auto view() const {
        return registry_.view<C0, C1, C2>();
    }

    template<typename C0, typename C1, typename C2, typename C3>
    [[nodiscard]] auto view() const {
        return registry_.view<C0, C1, C2, C3>();
    }

    /**
     * VIEW WITH EXCLUSION — and the fixed arities make it CALLABLE for the first time.
     *
     * The previous signature was `template<typename... Include, typename... Exclude>`. Two
     * consecutive parameter packs cannot be filled: the first one absorbs every explicit
     * argument, so `view_exclude<A, B>()` deduced Include={A,B} and Exclude={} — the exclusion
     * was unreachable by construction. That is why the measurement found zero callers; the
     * function was not merely unused, it was unusable. Naming the parameters separately is what
     * gives the caller a way to say which side each type belongs to.
     */
    template<typename Include0, typename Exclude0>
    [[nodiscard]] auto view_exclude() {
        return registry_.view<Include0>(entt::exclude<Exclude0>);
    }

    template<typename Include0, typename Include1, typename Exclude0>
    [[nodiscard]] auto view_exclude() {
        return registry_.view<Include0, Include1>(entt::exclude<Exclude0>);
    }

    template<typename Include0, typename Include1, typename Include2, typename Exclude0>
    [[nodiscard]] auto view_exclude() {
        return registry_.view<Include0, Include1, Include2>(entt::exclude<Exclude0>);
    }

    /** Signals/Events */

    /// Register callback for component construction
    template<typename Component>
    auto on_construct() {
        return registry_.on_construct<Component>();
    }

    /// Register callback for component destruction
    template<typename Component>
    auto on_destroy() {
        return registry_.on_destroy<Component>();
    }

    /// Register callback for component update
    template<typename Component>
    auto on_update() {
        return registry_.on_update<Component>();
    }

    /*
     * WORLD'S SYSTEM MANAGEMENT STOOD HERE UNTIL 2026-08-20 AND IS DELETED, not moved.
     *
     * Eleven members went: add_system, add_system_ptr, get_system, tick, tick_up_to_phase,
     * tick_phase_range, tick_phase, system_count, systems, sort_systems, and the systems_
     * container itself. THIS WAS THE SECOND TIME THE FILE THAT THEN HELD THIS CLASS MADE THIS
     * EXACT CUT — see the dependencies() note in system.hpp, deleted the same day for the same
     * reason, with app.hpp carrying the successor.
     *
     * MEASURED BEFORE THE CUT, whole tree, that file excluded:
     *
     *     capability          users   equivalent in App
     *     add_system<T>         1     add_system<T>(Schedule)   richer: a Schedule, not an int
     *     tick(dt)              2     tick(dt)                  same
     *     tick_up_to_phase      0     run_schedule(Schedule,dt) the schedule model replaces
     *     tick_phase_range      0     run_schedule              the hand-rolled int phases
     *     tick_phase            0     run_schedule
     *     system_count          0     system_count()            same
     *     add_system_ptr        0     none
     *     get_system<T>         0     none
     *     systems()             0     none
     *
     * The only three users were core/ase-ecs/tests/test_ecs.cpp:180/188/197 — the ECS testing
     * the one path nothing else took. Production registers through App:
     * `app.add_system<X>(ecs::Schedule::…)`, kernel_module.hpp and 1725 module and plugin files.
     *
     * THE THREE MEMBERS WITHOUT AN App EQUIVALENT ARE ALSO THE THREE WITH ZERO USERS, so
     * nothing that was used went away. Positive control for that measurement: ecs::World itself
     * is used 38 times — the class lives, only this block did not.
     *
     * AND get_system<T> WAS NOT A CAPABILITY WORTH KEEPING: it was a dynamic_cast over a list
     * of base pointers, which is the OOP type dispatch the ECS rules forbid outright. Its
     * implementation WAS the violation. Keeping it would have meant carrying an anti-pattern to
     * preserve a call nobody makes.
     *
     * WHAT WENT WITH IT, and this is why the cut is worth its size: 4x std::unique_ptr, the one
     * dynamic_cast, and three tick() calls made through a base pointer — eight of the twelve
     * real findings in that file, none of them removed by rewriting a pattern.
     */

    /** Direct Registry Access */

    /// Get underlying registry (for advanced use)
    [[nodiscard]] Registry& registry() { return registry_; }
    [[nodiscard]] const Registry& registry() const { return registry_; }

private:
    Registry registry_;
};

}  // namespace ase::ecs
