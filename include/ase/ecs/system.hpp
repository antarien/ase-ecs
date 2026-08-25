#pragma once

/**
 * ASE ECS SYSTEM HEADER
 *
 * @file        system.hpp
 * @brief       ECS foundation - type aliases, the System base class and the World facade
 * @description Thin wrapper around EnTT providing the Entity Component System foundation.
 *              All game objects (chunks, entities, players) are ECS entities with components.
 *
 *              THIS FILE IS NOT A SYSTEM, AND TWO META FIELDS BELOW SAY SO ONLY BY CONVENTION.
 *              It DEFINES the System base class; it registers nothing and ticks in no schedule.
 *              `@schedule Initialization` is the least wrong of the thirty valid names — the
 *              World it declares is built at startup — but it describes no running system, and
 *              no reader should look for one. The same holds for the checklist below: it is
 *              required of every file whose name ends in `system`, and several of its lines
 *              cannot apply to a header that declares a base class and a facade (World does
 *              hold `registry_` as a member, on purpose).
 *
 *              WHY THIS FILE CARRIES A SYSTEM HEADER AT ALL, measured 2026-08-22: its first
 *              commit is eaa8b12 of 2026-01-15, "Refactor ECS: Rename ecs.hpp to system.hpp".
 *              The validator keys a file's type on the name ENDING in `system`/`sys`, so that
 *              rename silently moved this file from one rule set into another and added the
 *              header requirements it had never been written for. The name stays as it is:
 *              renaming it back would change the rule set again, which is exactly the move
 *              Rule 11 forbids.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    structure/datatype
 * @schedule    Initialization
 * @created     2026-01-15
 * @modified    2026-08-22
 * @version     1.0.0
 *
 * ECS SYSTEM HEADER COMPLIANCE
 *
 * [ ] STATELESS - No member variables
 * [ ] Views created on demand, not stored
 * [ ] NO direct calls to other systems
 * [ ] Communication only via Components
 * [ ] Helpers in anonymous namespace (in .cpp, NOT static functions!)
 * [ ] Math functions from ase-math (Layer 0)
 * [ ] NO file-level static/constexpr (constants → types.hpp)
 * [ ] Registered in Module with correct Schedule
 * [ ] Filename matches convention
 * [ ] Class name derived from filename
 * [ ] ALL THREE METHODS DECLARED: on_start, tick, on_stop
 *
 * Usage:
 *   #include <ase/ecs/system.hpp>
 *
 * THAT LINE SAID <ase/ecs/ecs.hpp> UNTIL 2026-08-20, AND THAT FILE DOES NOT EXIST.
 * Measured: `find core/ase-ecs -name ecs.hpp` returns nothing, and ase/ecs/ holds app.hpp,
 * plugin_interface.hpp, schedule.hpp and this file. The only places following the old advice
 * are 20 validator test fixtures, which are written to fail on purpose and never compiled -
 * so nothing live ever broke on it, and nothing ever would have reported it either. A usage
 * example is read far more often than it is tried; this one pointed at a header for long
 * enough that a reader could only conclude their include path was wrong.
 *
 *   ase::ecs::World world;
 *   auto entity = world.create();
 *   world.emplace<Position>(entity, 0.0f, 0.0f, 0.0f);
 *   world.emplace<Velocity>(entity, 1.0f, 0.0f, 0.0f);
 *
 *   // Systems iterate over entities with specific components
 *   world.view<Position, Velocity>().each([](auto entity, auto& pos, auto& vel) {
 *       pos.x += vel.x * dt;
 *   });
 *
 * WHERE THE SCHEDULE-BASED EXECUTION LIVES - and why there is no system.cpp:
 *
 *   World implementation is now minimal - all schedule-based execution is
 *   handled by App (app.cpp). World only provides registry access and basic
 *   system management for direct use.
 *
 * That sentence stood in src/system.cpp, a translation unit of nine lines that
 * defined no symbol at all and carried nothing but this explanation. The file
 * was deleted on 2026-08-20 under Rule 15; the sentence was moved here first,
 * because it is the answer to the question the empty file used to raise. Anyone
 * looking for the tick loop finds it in app.cpp, not below this header.
 */

#include <entt/entt.hpp>
#include <cstdint>
#include <type_traits>
// <algorithm> and <memory> WENT WITH World's system management: std::sort lived in
// sort_systems(), std::unique_ptr and std::make_unique in the systems_ container. Nothing else
// in this file uses either.
//
// MEASURED BEFORE REMOVING THEM, because this header is included by 1542 files and whoever uses
// a symbol WITHOUT its own include gets it from here for free - a coupling no gate can see:
//     <memory>      0 files at risk (2 bring it themselves)
//     <algorithm>   0 files at risk
// The first run of that measurement said FIVE files were at risk for <memory>, and all five
// were checklist lines reading "[ ] NO std::shared_ptr in components" - the counter had read
// COMMENTS as usage, which is exactly the blind spot this session reported in the validator
// the same afternoon. The number above is from the corrected run, with comments masked.
//
// ase/containers/vector.hpp STAYS although this file no longer uses it: the same measurement
// found THREE files that use ase::containers::Vector without their own include -
// modules/ase-replication/tests/test_replication_cred.cpp, .../test_replication_rsn.cpp and
// modules/ase-network/tests/network_test.cpp. Removing it here would break all three, and it
// carries no violation. Whoever owns those tests should add the include; then this line can go.
#include <ase/containers/vector.hpp>

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
 * ARITY FOUR matches the World::view() ladder below so the two do not diverge. Whoever needs a
 * fifth adds one line here and one overload there.
 */
template<typename C0>
using View1 = entt::view<entt::get_t<C0>>;

template<typename C0, typename C1>
using View2 = entt::view<entt::get_t<C0, C1>>;

template<typename C0, typename C1, typename C2>
using View3 = entt::view<entt::get_t<C0, C1, C2>>;

template<typename C0, typename C1, typename C2, typename C3>
using View4 = entt::view<entt::get_t<C0, C1, C2, C3>>;

/** System Base */

/**
 * Base class for ECS systems
 *
 * Systems contain logic that operates on entities with specific components.
 * Override tick() to implement system behavior.
 *
 * Systems are registered via the App builder:
 *   app.add_system<MySystem>(Schedule::Dynamics);
 */
class System {
public:
    virtual ~System() = default;

    /// System name for debugging/profiling
    [[nodiscard]] virtual const char* name() const = 0;

    /// Called every frame/tick
    virtual void tick(Registry& registry, float dt) = 0;

    /// Called on system startup
    virtual void on_start(Registry& /*registry*/) {}

    /// Called on system shutdown
    virtual void on_stop(Registry& /*registry*/) {}

    /// System phase (for boot order) - set by SystemRegistry
    [[nodiscard]] int phase() const { return phase_; }
    void set_phase(int phase) { phase_ = phase; }

    /// System priority within phase (lower = runs first)
    [[nodiscard]] virtual int priority() const { return 0; }

    /*
     * dependencies() STOOD HERE UNTIL 2026-08-20 - "system names that must start before
     * this one", a virtual returning an empty vector.
     *
     * IT WAS THE PREDECESSOR OF run_after AND HAD BEEN DEAD SINCE THAT ARRIVED. Measured:
     * zero overrides and zero callers in the whole tree, while app.hpp
     * (SystemBuilder::run_after) carries the same job in 1725 module and plugin files. The
     * successor states the order where the registration happens, which is where a reader
     * looks for it; the virtual stated it inside the system, where nobody asked.
     *
     * It is deleted rather than kept as a seam: a virtual that no one overrides is not an
     * extension point, it is a promise the base class cannot keep. Its std::string return
     * was also the last string in this header.
     */

    /// Is system enabled?
    [[nodiscard]] bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
    int phase_ = 0;
};

/** World (Registry + Systems) */

/**
 * The World contains all entities and runs systems
 *
 * This is the central object that owns the ECS registry and manages
 * system execution order.
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
     * that file is this header — no caller exists. GESETZT is four, matching the view ladder
     * below, so the two do not diverge for a reader who uses them together.
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
     * example at the top of this file), and their **highest arity is two**. The 33419 calls
     * that reach `registry.view<>()` directly do NOT come through here — they go to EnTT, whose
     * pack is not this module's to change; measuring them together is what first suggested a
     * ladder up to nineteen for seven call sites.
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
     * container itself. THIS IS THE SECOND TIME THIS FILE MAKES THIS EXACT CUT — see the
     * dependencies() note above, deleted the same day for the same reason, with app.hpp
     * carrying the successor.
     *
     * MEASURED BEFORE THE CUT, whole tree, this file excluded:
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
     * real findings in this file, none of them removed by rewriting a pattern.
     */

    /** Direct Registry Access */

    /// Get underlying registry (for advanced use)
    [[nodiscard]] Registry& registry() { return registry_; }
    [[nodiscard]] const Registry& registry() const { return registry_; }

private:
    Registry registry_;
};

/*
 * COMMON COMPONENT TAGS — MOVED OUT ON 2026-08-22, one header each:
 *
 *     DestroyTag      →  ase/ecs/components/tag/ecs_dstr_tag.hpp       EcsDstrTag
 *     DirtyTag        →  ase/ecs/components/tag/ecs_dty_tag.hpp        EcsDtyTag
 *     CreatedTag      →  ase/ecs/components/tag/ecs_crtd_tag.hpp       EcsCrtdTag
 *     PendingSyncTag  →  ase/ecs/components/tag/ecs_pend_sync_tag.hpp  EcsPendSyncTag
 *
 * They carried the Tag suffix since 2026-08-20; what they lacked was a place. The rule
 * `struct in System files forbidden` reached them only after the 2026-01-15 rename of
 * ecs.hpp to system.hpp moved this file into the system-header rule set — the structs had
 * not changed, the rules around them had.
 *
 * THE FORM IS NOT INVENTED HERE: ase-neo4j and ase-serial keep twelve tags between them under
 * components/tag/, one per file, each named {module}_{tax}_tag.hpp with the module prefix in
 * the struct name. Measured 2026-08-22: in all of core/ and kernel/ exactly four tags lacked
 * that prefix, and they were these four.
 *
 * THIS HEADER DELIBERATELY DOES NOT INCLUDE THEM. Whoever uses a tag includes its header —
 * otherwise every consumer of system.hpp keeps receiving all four for free, which is the
 * coupling the move was meant to end. The five files that used them were changed in the same
 * pass; a use without its own include compiles today and breaks the day this file stops
 * pulling it in.
 */

/*
 * CommandMessage STOOD HERE UNTIL 2026-08-20 AND IS DELETED, not moved.
 *
 * It was a component with two std::string fields, and its comment described a working
 * flow: "Server creates entity with this component from REST API. Plugin systems query for
 * their command type and process. System destroys entity after processing." Measured over
 * the whole tree, comments and log strings excluded: ZERO producers, ZERO consumers, and no
 * server implements the POST /api/command route the example named. The only thing in the
 * tree that still knows that route is the client's API tester
 * (clients/ase-client-web/ase-web-api-tester/src/types/api-tester.types.ts:133-135), which
 * offers three commands against an endpoint nobody serves.
 *
 * So the comment was not documentation, it was a description of something that was never
 * built - and a reader of the ECS core had no way to tell. Rule 15 decides the rest:
 * greenfield, no external consumers, dead code is deleted rather than carried.
 *
 * WHAT A FUTURE COMMAND SEAM MUST NOT REPEAT: two std::string in a component. A command
 * name is a type identifier and belongs in a uint32_t from entt::hashed_string; a JSON
 * payload of unbounded length belongs behind a lookup id in registry.ctx(), not inline in
 * the row. That is what the component rules say, and it is why this struct could never have
 * passed the gate in the shape it had.
 */

/** Entity ID Utilities */

/// Get numeric ID from entity
[[nodiscard]] inline uint32_t entity_id(Entity entity) {
    return static_cast<uint32_t>(entt::to_integral(entity));
}

/// Get entity version
[[nodiscard]] inline uint32_t entity_version(Entity entity) {
    return entt::to_version(entity);
}

}  // namespace ase::ecs
