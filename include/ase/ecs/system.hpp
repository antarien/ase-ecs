#pragma once

/**
 * ASE ECS SYSTEM HEADER
 *
 * @file        system.hpp
 * @brief       The System base class - and the header every ECS consumer still includes
 * @description Declares the base class a system author derives from, and keeps the ECS
 *              vocabulary and the World facade reachable under the include path the tree has
 *              always used.
 *
 *              THIS FILE IS NOT A SYSTEM, AND TWO META FIELDS BELOW SAY SO ONLY BY CONVENTION.
 *              It DEFINES the System base class; it registers nothing and ticks in no schedule.
 *              `@schedule Initialization` is the least wrong of the thirty valid names — the
 *              World it makes reachable is built at startup — but it describes no running
 *              system, and no reader should look for one. The same holds for the checklist
 *              below: it is required of every file whose name ends in `system`, and several of
 *              its lines cannot apply to a header that declares a base class.
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
 * @modified    2026-08-30
 * @version     1.0.0
 *
 * WHAT LEFT THIS FILE ON 2026-08-30, AND WHY NOTHING BREAKS
 *
 * It carried 625 lines and three purposes. A file in the unsplit band is evidence that a
 * separation is missing, and here the audiences were plain: a system author derives from System
 * and never touches World; a caller who builds a world never derives from System; and both, plus
 * every one of the 1962 files that include this header, need the type aliases.
 *
 *     ase/ecs/entity.hpp  Entity, NullEntity, Registry, Storage, View1..View4, the wire-contract
 *                         assertion, entity_id and entity_version. NOT called types.hpp: that
 *                         name makes a file its module's SSOT for CONSTANTS, and this one defines
 *                         none - the promise would have been written to satisfy a template.
 *     ase/ecs/world.hpp   class World - the fixed-arity facade over one Registry.
 *
 * BOTH ARE INCLUDED BELOW, ON PURPOSE. Every name this header offered before it is still
 * reachable through it, so no include line anywhere in the tree changes. That is what makes a
 * cut at Layer 1 safe: the outward interface stays, only the inner structure moves. A reader who
 * wants just the vocabulary may include types.hpp directly; nobody has to.
 *
 * THE VOCABULARY IS A FILE OF ITS OWN rather than living with System or with World, because both
 * of those need it. Leaving it in either would force the other to include it, and two headers
 * that include each other are not a seam.
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
 */

// The vocabulary both this file and world.hpp are written in.
#include <ase/ecs/entity.hpp>
// Included so that every existing reader of system.hpp keeps seeing World unchanged.
#include <ase/ecs/world.hpp>

// ase/containers/vector.hpp STAYS although this file does not use it: a measurement on
// 2026-08-22 found THREE files that use ase::containers::Vector without their own include -
// modules/ase-replication/tests/test_replication_cred.cpp, .../test_replication_rsn.cpp and
// modules/ase-network/tests/network_test.cpp. Removing it here would break all three, and it
// carries no violation. Whoever owns those tests should add the include; then this line can go.
#include <ase/containers/vector.hpp>

namespace ase::ecs {

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

}  // namespace ase::ecs
