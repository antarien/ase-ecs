#pragma once

/**
 * ASE ECS SYSTEM HEADER
 *
 * @file        system.hpp
 * @design      DSGN_016
 * @brief       The System base class every ECS system derives from
 * @description Declares the base class a system author derives from: name, tick, on_start,
 *              on_stop, the boot phase and the enabled switch. It includes the ECS vocabulary it
 *              is written in and nothing else.
 *
 *              THIS FILE IS NOT A SYSTEM, AND TWO META FIELDS BELOW SAY SO ONLY BY CONVENTION.
 *              It DEFINES the System base class; it registers nothing and ticks in no schedule.
 *              `@schedule Initialization` is the least wrong of the valid names, because systems
 *              are started at startup, but it describes no running system, and no reader should
 *              look for one. The same holds for the checklist below: the validator keys a file's
 *              type on a name ending in `system`, so it requires the checklist of a system
 *              header here, and several of its lines cannot apply to a header that declares a
 *              base class.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    structure/datatype
 * @schedule    Initialization
 * @usecase     usecase:100232 - author ecs system
 * @created     2026-01-15
 * @modified    2026-10-11
 * @version     1.1.0
 *
 * WHAT THIS HEADER GIVES AND WHAT IT DOES NOT
 *
 * A system author derives from System and reads the Registry it is handed; the vocabulary
 * (Entity, NullEntity, Registry, Storage, View1..View4, entity_id, entity_version) comes from
 * ase/ecs/entity.hpp, which this header includes because System is written in it.
 *
 * The World facade lives in ase/ecs/world.hpp and the containers in ase/containers/. This header
 * does not include them: a file that names World or Vector includes their header itself. A header
 * included by every system of a tier rebuilds the whole tier on every change below it, so it
 * carries only what its own declarations need.
 *
 * The common component tags (EcsDstrTag, EcsDtyTag, EcsCrtdTag, EcsPendSyncTag) stand one per
 * header under ase/ecs/components/tag/. Whoever uses a tag includes its header.
 *
 * Build order between systems is stated where they are registered, with
 * SystemBuilder::run_after in app.hpp, never inside the system.
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
 *   class MovementSystem : public ase::ecs::System {
 *   public:
 *       const char* name() const override { return "MovementSystem"; }
 *       void on_start(Registry& registry) override { (void)registry; }
 *       void tick(Registry& registry, float dt) override {
 *           for (auto [entity, pos, vel] : registry.view<Position, Velocity>().each())
 *               pos.x += vel.x * dt;
 *       }
 *       void on_stop(Registry& registry) override { (void)registry; }
 *   };
 */

// The vocabulary System is written in: Registry for tick, on_start and on_stop.
#include <ase/ecs/entity.hpp>

namespace ase::ecs {

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

    /// Is system enabled?
    [[nodiscard]] bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
    int phase_ = 0;
};

}  // namespace ase::ecs
