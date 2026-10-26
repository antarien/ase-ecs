#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app.hpp
 * @brief       Application assembly and the tick loop that drives it
 * @description Declares App, the object a process builds a world with: kernel,
 *              modules and plugins register their systems into it, and it runs
 *              the schedules through TickScheduler. Everything above it
 *              registers systems, everything below it stores or sorts them.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    ecs/module
 * @usecase     usecase:100357 - run the tick loop
 * @actor       Entity Component System (ECS)
 * @created     2025-12-01
 * @modified    2026-10-25
 * @version     1.4.0
 *
 * Usage:
 *   ecs::App()
 *       .add_kernel<ase::kernel::Kernel>()
 *       .add_module<PlayerModule>()
 *       .add_plugin<SkyPlugin>()
 *       .run();
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

#include "schedule.hpp"
#include "system.hpp"
// App holds its World by value, so the full definition is needed here.
#include "world.hpp"
#include "internal/system_registry.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <ase/containers/hash_map.hpp>
#include <ase/containers/int_hash.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs {

// VERSION DETECTION HELPER

namespace detail {

/* THE DETECTION STANDS WHERE IT IS USED, as one requires expression: can T::version() be
 * written. No trait type, no partial specialisation, no void_t - the house form of this file,
 * the same the concepts Kernel, Module and Plugin below are built in. `if constexpr` chooses at
 * compile time: whoever has version() gets it, whoever has not, the empty string.
 */

// Get version if available, otherwise return empty string
template<typename T>
const char* get_version() {
    if constexpr (requires { T::version(); }) {
        return T::version();
    } else {
        return "";
    }
}

}  // namespace detail

// Planes of the region domain gate (App::run_schedule). EXACTLY TWO, operator decision of
// 2026-08-26: the default of every module and plugin is SIMULATION - without RegionRect coverage
// it is held, because a node without active chunks simulates NOTHING (VIS_ASE.md: chunk model and
// sphere frequencies; weather and celestial are simulation too and run only with coverage, in
// their slow schedules). The one exception is the infrastructure, which is no simulation. The
// declaration comes DATA-DRIVEN from the module manifest (module.toml `plane`, declared by the
// kernel before loading), never from code. The gate works on the SPACE axis (coverage) only -
// never on project or tenant axes: the silo model is forbidden (ARCH_ASE_TOPOLOGY.md), the graph
// is ONE, and wanderers change their simulator by region handoff (the ReplicaHoffFlipSystem
// chain), never their state.
constexpr uint8_t APP_PLANE_SIM  = 0;  // region-bound simulation (default of every module)
constexpr uint8_t APP_PLANE_KERN = 1;  // infrastructure: always ticks (network, persistence, kernel)

// Who owns the PROCESS this App runs in. EXACTLY TWO cases, both with a place in the tree: the
// five tier servers, whose main() IS the App - it blocks SIGINT/SIGTERM/SIGHUP/SIGPIPE for the
// whole process, spins its own loop in run() and ends the process with _exit(0); and an embedding
// host that owns process, loop, signals and terminal (Godot in the Vivarium client,
// PLAN_ASE_VIVARIUM_PHASE_00_CONTRACT section 00.1). There the App is a guest: startup/tick/
// shutdown on call, no signal watch, no change of the signal mask, no progress table, no run().
// The simulation itself is the same in both cases.
constexpr uint8_t APP_LIFE_PROCESS = 0;  // the App owns the process (tier server, default)
constexpr uint8_t APP_LIFE_HOST    = 1;  // a host owns the process (embedded App)

// Forward declarations
class App;
class SystemBuilder;

namespace internal {
class TickScheduler;
class SystemRegistry;
}  // namespace internal

// KERNEL CONCEPT (Layer 2) - core kernel, foundation for modules/plugins

template<typename T>
concept Kernel = requires(T kernel, App& app) {
    { kernel.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// MODULE CONCEPT (Layer 3) - core game systems, statically linked

template<typename T>
concept Module = requires(T module, App& app) {
    { module.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// PLUGIN CONCEPT (Layer 4) - optional, hot-loadable features

template<typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// SYSTEMBUILDER - fluent API for system configuration

class SystemBuilder {
public:
    SystemBuilder(App& app, Schedule schedule, std::unique_ptr<System> system);

    SystemBuilder& run_after(std::string_view name);
    SystemBuilder& with_priority(int priority);
    App& done();

    // Auto-finalize when destroyed without calling done()
    ~SystemBuilder();

private:
    App& app_;
    Schedule schedule_;
    std::unique_ptr<System> system_;
    ase::containers::Vector<std::string> after_;
    std::string source_;
    std::string version_;
    int priority_ = 0;
    bool finalized_ = false;
};

// APP - application builder

class App {
public:
    // The time axis runs over utils::monotonic_nanos(): one monotonic source as int64_t
    // nanoseconds, no family of clock types. clock.hpp names where a duration, a deadline or a
    // frame time belongs and where a calendar date belongs (wall_time_seconds()).

    /** Process-owned App (APP_LIFE_PROCESS) - the form every tier server builds. */
    App();

    /**
     * App with an explicit owner of the process.
     *
     * @param lifecycle APP_LIFE_PROCESS (same as App()) or APP_LIFE_HOST for an App embedded in
     *                  a host that owns process, loop, signals and terminal. Host-owned, startup()
     *                  arms no signal watch and leaves the signal mask untouched, neither sequence
     *                  draws a terminal table, shutdown() hands the log sinks back, and run() is
     *                  refused - the host drives startup/tick/shutdown itself.
     */
    explicit App(uint8_t lifecycle);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // BUILDER API

    /**
     * Set current source for system registration logging.
     */
    App& set_source(std::string_view source) {
        current_source_ = source;
        return *this;
    }

    /**
     * Declare the region-domain-gate plane of one module BEFORE loading it (the kernel
     * reads the value from the module manifest). Undeclared modules default to
     * APP_PLANE_SIM and are held by run_schedule while no RegionRect coverage stands.
     */
    App& declare_module_plane(std::string_view module, uint8_t plane);

    /**
     * Set boot delay per system in microseconds (default: 0).
     * Use for visual boot sequence effect.
     */
    App& set_boot_delay(int delay_us) {
        boot_delay_us_ = delay_us;
        return *this;
    }

    /**
     * Set shutdown delay per system in microseconds (default: 0).
     * Use for visual shutdown sequence effect.
     */
    App& set_shutdown_delay(int delay_us) {
        shutdown_delay_us_ = delay_us;
        return *this;
    }

    /**
     * Add the kernel (Layer 2 - Foundation).
     * Must be called first, before modules and plugins.
     */
    template<Kernel K>
    App& add_kernel() {
        K kernel;
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = K::name();
        current_version_ = detail::get_version<K>();
        // Kernel systems are infrastructure: never held by the region domain gate.
        declare_module_plane(K::name(), APP_PLANE_KERN);
        kernel.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    /**
     * Add the kernel with ONE constructor argument forwarded to K.
     * Used to pass tier identity into the kernel at setup time,
     * e.g. app.add_kernel<Kernel>(Tier::Replica).
     *
     * ONE FIXED PARAMETER INSTEAD OF A PACK, and the arity is measured, not guessed: across the
     * tree there are exactly FIVE call sites of this overload - the five main.cpp of the tiers -,
     * and each passes exactly one argument (ase::kernel::Tier::X). The pack never carried anything
     * else, so the fixed parameter costs no call site.
     *
     * Should a kernel one day need two arguments, the answer is NOT the pack again but a type
     * that carries both - then the call names what is passed, instead of a positional list one
     * has to count while reading.
     */
    template<Kernel K, typename Arg>
    App& add_kernel(Arg&& arg) {
        K kernel(std::forward<Arg>(arg));
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = K::name();
        current_version_ = detail::get_version<K>();
        // Kernel systems are infrastructure: never held by the region domain gate.
        declare_module_plane(K::name(), APP_PLANE_KERN);
        kernel.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    /**
     * Add a plugin (Layer 4 - Optional features).
     */
    template<Plugin P>
    App& add_plugin() {
        P plugin;
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = P::name();
        current_version_ = detail::get_version<P>();
        plugin.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    template<Plugin P>
    App& add_plugin(P&& plugin) {
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = std::decay_t<P>::name();
        current_version_ = detail::get_version<std::decay_t<P>>();
        plugin.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    /**
     * Add a module (Layer 3 - Core game systems).
     */
    template<Module M>
    App& add_module() {
        M module;
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = M::name();
        current_version_ = detail::get_version<M>();
        module.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    template<Module M>
    App& add_module(M&& module) {
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = std::decay_t<M>::name();
        current_version_ = detail::get_version<std::decay_t<M>>();
        module.build(*this);
        current_source_ = std::move(prev_src);
        current_version_ = std::move(prev_ver);
        return *this;
    }

    /**
     * Add a system to a schedule (simple version).
     */
    template<typename S>
    App& add_system(Schedule schedule) {
        auto system = std::make_unique<S>();
        finalize_system(schedule, std::move(system), {}, 0, current_source_, current_version_);
        return *this;
    }

    /**
     * Add a system with builder for dependencies.
     * Usage: app.add_system_with<MySystem>(Schedule::Integration).run_after("OtherSystem").done();
     */
    template<typename S>
    SystemBuilder add_system_with(Schedule schedule) {
        auto system = std::make_unique<S>();
        return SystemBuilder(*this, schedule, std::move(system));
    }

    /**
     * Set current version for system registration.
     */
    App& set_version(std::string_view version) {
        current_version_ = version;
        return *this;
    }

    /**
     * Internal: Called by SystemBuilder to finalize system addition.
     */
    void finalize_system(Schedule schedule, std::unique_ptr<System> system,
                         ase::containers::Vector<std::string> after, int priority,
                         std::string source = {}, std::string version = {});

    const std::string& current_source() const { return current_source_; }
    const std::string& current_version() const { return current_version_; }

    // LIFECYCLE

    /**
     * Full run: startup + main loop + shutdown, then the process ends with _exit(0).
     * The form of the five tier servers. A host-owned App (APP_LIFE_HOST) refuses it with an
     * error line and returns without touching the App: the loop and the process belong to the
     * host, which drives startup/tick/shutdown itself.
     */
    void run();

    /**
     * Initialize systems and run Startup schedule.
     * Call this before manual tick loop.
     */
    void startup();

    /**
     * Run one frame of the main loop schedules: the real-time tiers, then the game-time events
     * requested through EcsAppReqSchdComponent, then Conclusion (ARCH_ASE_SCHEDULE section 6.3).
     * Call this in your manual tick loop.
     */
    void tick(float dt);

    /**
     * Run Shutdown schedule.
     * Call this after manual tick loop ends.
     */
    void shutdown();

    /**
     * Run a specific schedule manually.
     *
     * Ending runs as an EVENT that tick() picks up: SIGINT/SIGTERM over the signalfd watch, a
     * functional stop over EcsAppQuitReqTag (producer: KernelCoreStopSystem, Schedule::Conclusion).
     * There is no quit()/is_running() pair and no server loop of its own around it - all five
     * tiers call run().
     */
    void run_schedule(Schedule schedule, float dt);

    // WORLD ACCESS

    World& world() { return world_; }
    const World& world() const { return world_; }
    Registry& registry() { return world_.registry(); }
    const Registry& registry() const { return world_.registry(); }

    // INTROSPECTION API (port of systemRegistry.ts getSystemCount/getAllSystems)

    /** Total number of registered systems across all schedules */
    size_t system_count() const;

    /**
     * How often the tick scheduler ran one schedule since this App was built.
     *
     * Counted by the scheduler where the schedule's tier runs, never derived from simulated
     * time - a tier that missed its interval or caught up shows in the count. A game-time
     * schedule counts each event the drain fired for it. An embedding host reads it for its
     * measurement line (the Vivarium client: Regulation steps).
     *
     * @param schedule the schedule to look up
     * @return the number of runs; 0 before the first tick, for a schedule the scheduler does
     *         not drive (lifecycle schedules) and for a game-time schedule no event has fired yet
     */
    [[nodiscard]] uint64_t schedule_runs(Schedule schedule) const;

    /**
     * The longest frame tick() takes as it is, read from the tick scheduler.
     *
     * A longer dt is clamped to this length and the remainder is LOST, not deferred. A host that
     * owns the loop and has to make up a long real span - the embedded host after the app comes
     * back from the background - therefore ticks in steps of exactly this length
     * (KernelEmbeddedHost::advance), so every schedule runs as often as the span demands and no
     * module computes a giant step.
     *
     * @return the frame-time ceiling in seconds
     */
    [[nodiscard]] float frame_time_max() const;

    /** Get all system metadata (name, source, schedule, run_after) */
    const ase::containers::Vector<internal::SystemInfo>& system_infos() const;

    /** Get systems for a specific schedule */
    const ase::containers::Vector<std::unique_ptr<System>>& systems_for(Schedule schedule) const;

    /** Set callback invoked during shutdown (port of setOnDestroyCallback) */
    void set_on_destroy(void(*callback)()) { on_destroy_callback_ = callback; }

    /**
     * Set callback invoked ONCE in shutdown(), after the on_stop of the LAST system returned and
     * while the App still exists. set_on_destroy fires at the START of shutdown, before
     * Finalization; this one fires at its END. The kernel unloads its modules here: no system
     * runs any more, so no on_stop can meet a module that was already cleaned up.
     */
    void set_on_stopped(void(*callback)(App& app)) { on_stopped_callback_ = callback; }

    /** Access SystemRegistry for Hot-Reload (remove_systems_by_source) */
    internal::SystemRegistry& system_registry() { return *system_registry_; }

    /**
     * Pass CLI args from main(). Stored for KernelCmdSystem to parse.
     * L5 passes, L2 processes — clean layer separation.
     */
    void set_cli_args(int argc, char* argv[]) { cli_argc_ = argc; cli_argv_ = argv; }
    int cli_argc() const { return cli_argc_; }
    char** cli_argv() const { return cli_argv_; }

    /** Owner of the process this App runs in: APP_LIFE_PROCESS or APP_LIFE_HOST. */
    [[nodiscard]] uint8_t lifecycle() const { return lifecycle_; }

private:
    /**
     * Entry point the tick scheduler calls back into, once per due schedule.
     *
     * Static with a void* because ScheduleRunner is a plain function pointer (see
     * tick_scheduler.hpp for why). `user` is the App that started the tick; this function does
     * nothing but recover it and hand the call to the member below. The measuring itself lives
     * in run_schedule_measured so the trampoline stays the one line it should be.
     */
    static void schedule_trampoline(void* user, Schedule schedule, float sched_dt);

    /** Same shape for the game-time drain of one tick: recovers the App and runs its events. */
    static void event_trampoline(void* user);

    /** Same shape for the shutdown sequence: recovers the App and hands it to on_stopped_callback_. */
    static void stopped_trampoline(void* user);

    /** Runs one schedule; brackets Dynamics with the wall-clock measurement for kernel stats. */
    void run_schedule_measured(Schedule schedule, float sched_dt);

    /**
     * Fires the game-time events requested in this tick, in sequence order, each with its game
     * seconds as dt, and destroys every request it took (ARCH_ASE_SCHEDULE section 6.3).
     */
    void run_game_events();

    World world_;

    // Internal components (PIMPL for clean separation)
    std::unique_ptr<internal::TickScheduler> tick_scheduler_;
    std::unique_ptr<internal::SystemRegistry> system_registry_;

    std::atomic<bool> running_{false};
    int64_t last_frame_time_ = 0;  // monotonic nanoseconds, set in startup()
    // M-B module axis: O(1) index (mod_hash<<32 | sched_id) → row entity of the
    // EcsAppStaModTimComponent upsert in run_schedule. IntMixHash is mandatory
    // for integer keys (ARCH_ASE_HUB_ASYNC 9.7).
    ase::containers::HashMap<uint64_t, Entity, ase::containers::IntMixHash> mod_tim_rows_;
    // Flip memory of the region domain gate: -1 unknown, 0 running, 1 held. The gate itself is
    // stateless per pass (anchor tag + RegionRect pool size); this only keeps the transition from
    // being logged again in every pass.
    int region_hold_state_ = -1;
    std::string current_source_;
    std::string current_version_;
    int boot_delay_us_ = 0;
    int shutdown_delay_us_ = 0;
    void(*on_destroy_callback_)() = nullptr;
    void(*on_stopped_callback_)(App& app) = nullptr;
    int cli_argc_ = 0;
    char** cli_argv_ = nullptr;
    // Fixed from the constructor on: who owns the process is not decided while running.
    uint8_t lifecycle_ = APP_LIFE_PROCESS;
};

// SYSTEMBUILDER IMPLEMENTATION (inline)

inline SystemBuilder::SystemBuilder(App& app, Schedule schedule, std::unique_ptr<System> system)
    : app_(app), schedule_(schedule), system_(std::move(system)),
      source_(app.current_source()), version_(app.current_version()) {}

inline SystemBuilder& SystemBuilder::run_after(std::string_view name) {
    after_.emplace_back(name);
    return *this;
}

inline SystemBuilder& SystemBuilder::with_priority(int priority) {
    priority_ = priority;
    return *this;
}

inline App& SystemBuilder::done() {
    if (!finalized_) {
        app_.finalize_system(schedule_, std::move(system_), std::move(after_), priority_,
                             std::move(source_), std::move(version_));
        finalized_ = true;
    }
    return app_;
}

inline SystemBuilder::~SystemBuilder() {
    if (!finalized_ && system_) {
        app_.finalize_system(schedule_, std::move(system_), std::move(after_), priority_,
                             std::move(source_), std::move(version_));
    }
}

}  // namespace ase::ecs
