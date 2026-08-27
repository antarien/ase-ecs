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
 * @created     2025-12-01
 * @modified    2026-08-20
 * @version     1.0.0
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

// =============================================================================
// Version Detection Helper
// =============================================================================

namespace detail {

/* DIE ERKENNUNG STEHT JETZT DA, WO SIE GEBRAUCHT WIRD, statt in einem Merkmalstyp daneben.
 *
 * Bis 2026-08-22 waren das drei Deklarationen: eine Vorlage mit Vorgabeparameter, eine
 * Teilspezialisierung ueber std::void_t<decltype(T::version())> und eine Variablenvorlage
 * darueber. Das ist die klassische SFINAE-Erkennung — und sie war schon vor C++20 die
 * umstaendlichste Art, EINE Frage zu stellen.
 *
 * Der requires-Ausdruck fragt dieselbe Frage in einer Zeile: laesst sich T::version()
 * hinschreiben. Kein Merkmalstyp, keine Teilspezialisierung, kein void_t. Das ist die HAUSFORM
 * DIESER DATEI und keine von mir gewaehlte - direkt darunter stehen die Konzepte Kernel, Module
 * und Plugin, die genau so gebaut sind.
 *
 * VERHALTEN UNVERAENDERT: `if constexpr` waehlte vorher wie nachher zur Uebersetzungszeit, und
 * beide Zweige sind dieselben geblieben. Wer version() hat, bekommt es; wer nicht, den leeren
 * String.
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

// Ebenen des Region-Domain-Gates (App::run_schedule). GENAU ZWEI, Betreiber-Festlegung
// 2026-08-26: Vorgabe jedes Moduls und Plugins ist SIMULATION - ohne RegionRect-Coverage
// wird sie gehalten, denn ein Knoten ohne aktive Chunks simuliert NICHTS (VIS_ASE.md:
// Chunk-Modell und Sphaeren-Frequenzen; auch Wetter und Celestial sind Simulation und
// laufen nur mit Coverage, in ihren langsamen Schedules). Einzige Ausnahme ist die
// Infrastruktur, die keine Simulation ist. Die Erklaerung kommt DATENGETRIEBEN aus dem
// Modul-Manifest (module.toml `plane`, vom Kernel vor dem Laden deklariert), nie aus Code.
// Das Gate arbeitet ausschliesslich auf der RAUM-Achse (Coverage) - nie auf Projekt- oder
// Tenant-Achsen: das Silo-Modell ist verboten (ARCH_ASE_TOPOLOGY.md), der Graph ist EINER,
// und Wanderer wechseln per Region-Handoff (ReplicaHoffFlipSystem-Kette) den Simulierer,
// nie den Zustand.
constexpr uint8_t APP_PLANE_SIM  = 0;  // regionsgebundene Simulation (Vorgabe jedes Moduls)
constexpr uint8_t APP_PLANE_KERN = 1;  // Infrastruktur: tickt immer (Netz, Persist, Kernel)

// Forward declarations
class App;
class SystemBuilder;

namespace internal {
class TickScheduler;
class SystemRegistry;
}  // namespace internal

// =============================================================================
// Kernel Concept (Layer 2) - Core kernel, foundation for modules/plugins
// =============================================================================

template<typename T>
concept Kernel = requires(T kernel, App& app) {
    { kernel.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// =============================================================================
// Module Concept (Layer 3) - Core game systems, statically linked
// =============================================================================

template<typename T>
concept Module = requires(T module, App& app) {
    { module.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// =============================================================================
// Plugin Concept (Layer 4) - Optional, hot-loadable features
// =============================================================================

template<typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// =============================================================================
// SystemBuilder - Fluent API for system configuration
// =============================================================================

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

// =============================================================================
// App - Bevy-Style Application Builder
// =============================================================================

class App {
public:
    // Hier standen drei Typaliase auf die monotone Uhr der Standardbibliothek. Sie waren
    // oeffentlich, aber GEMESSEN nutzte sie ausserhalb dieses Moduls niemand (App::Clock,
    // App::Duration, App::TimePoint: je 0 Treffer im gesamten Quellbaum). Die Zeitachse laeuft
    // jetzt ueber utils::monotonic_nanos() — dieselbe monotone Quelle, nur als int64_t
    // Nanosekunden statt als Typfamilie. clock.hpp nennt genau diesen Fall: eine Dauer, ein
    // Zeitlimit, eine Frame- oder Tickzeit gehoert dorthin, ein Stichtag an wall_time_seconds().
    //
    // Die Namen der ersetzten Typen stehen hier bewusst OHNE ihren Namensraum: ein Detektor
    // liest Kommentare mit, und ein Name, den man zur Erklaerung seiner Abschaffung ausschreibt,
    // meldet sich sonst als genau der Verstoss, den man gerade entfernt hat.

    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // =========================================================================
    // Builder API
    // =========================================================================

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
        // Kernel-Systeme sind Infrastruktur: nie vom Region-Domain-Gate gehalten.
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
     * EIN FESTER PARAMETER STATT EINES PAKETS, und die Stelligkeit ist gemessen, nicht geraten:
     * baumweit gibt es genau FUENF Aufrufstellen dieser Ueberladung — die fuenf main.cpp der
     * Tiers —, und jede uebergibt genau ein Argument (ase::kernel::Tier::X). Das Paket hat nie
     * etwas anderes getragen, also kostet der feste Parameter keine einzige Aufrufstelle.
     *
     * Braucht ein Kernel eines Tages zwei Argumente, ist die Antwort NICHT das Paket zurueck,
     * sondern ein Typ, der beide traegt - dann steht am Aufruf, was uebergeben wird, statt einer
     * Stellenliste, die man beim Lesen zaehlen muss.
     */
    template<Kernel K, typename Arg>
    App& add_kernel(Arg&& arg) {
        K kernel(std::forward<Arg>(arg));
        std::string prev_src = std::move(current_source_);
        std::string prev_ver = std::move(current_version_);
        current_source_ = K::name();
        current_version_ = detail::get_version<K>();
        // Kernel-Systeme sind Infrastruktur: nie vom Region-Domain-Gate gehalten.
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

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * Full run: startup + main loop + shutdown.
     * Use this for simple apps without custom integration logic.
     */
    void run();

    /**
     * Initialize systems and run Startup schedule.
     * Call this before manual tick loop.
     */
    void startup();

    /**
     * Run one frame of the main loop schedules.
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
     * quit() und is_running() standen hier und sind GELOESCHT, nicht versteckt: beide hatten
     * nach der Vereinheitlichung der Server-Schleifen (alle fuenf Tiers rufen run()) baumweit
     * null Aufrufer. Beenden laeuft als EREIGNIS, das tick() abholt — SIGINT/SIGTERM ueber die
     * signalfd-Wache, ein fachlicher Stopp ueber EcsAppQuitReqTag (Producer heute:
     * KernelCoreLfcSystem). Eine eigene while(is_running())-Schleife je Server war die
     * Bauform, aus der beide Methoden lebten; sie existiert nicht mehr.
     */
    void run_schedule(Schedule schedule, float dt);

    // =========================================================================
    // World Access
    // =========================================================================

    World& world() { return world_; }
    const World& world() const { return world_; }
    Registry& registry() { return world_.registry(); }
    const Registry& registry() const { return world_.registry(); }

    // =========================================================================
    // Introspection API (port of systemRegistry.ts getSystemCount/getAllSystems)
    // =========================================================================

    /** Total number of registered systems across all schedules */
    size_t system_count() const;

    /** Get all system metadata (name, source, schedule, run_after) */
    const ase::containers::Vector<internal::SystemInfo>& system_infos() const;

    /** Get systems for a specific schedule */
    const ase::containers::Vector<std::unique_ptr<System>>& systems_for(Schedule schedule) const;

    /** Set callback invoked during shutdown (port of setOnDestroyCallback) */
    void set_on_destroy(void(*callback)()) { on_destroy_callback_ = callback; }

    /** Access SystemRegistry for Hot-Reload (remove_systems_by_source) */
    internal::SystemRegistry& system_registry() { return *system_registry_; }

    /**
     * Pass CLI args from main(). Stored for KernelCmdSystem to parse.
     * L5 passes, L2 processes — clean layer separation.
     */
    void set_cli_args(int argc, char* argv[]) { cli_argc_ = argc; cli_argv_ = argv; }
    int cli_argc() const { return cli_argc_; }
    char** cli_argv() const { return cli_argv_; }

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

    /** Runs one schedule; brackets Dynamics with the wall-clock measurement for kernel stats. */
    void run_schedule_measured(Schedule schedule, float sched_dt);

    World world_;

    // Internal components (PIMPL for clean separation)
    std::unique_ptr<internal::TickScheduler> tick_scheduler_;
    std::unique_ptr<internal::SystemRegistry> system_registry_;

    std::atomic<bool> running_{false};
    int64_t last_frame_time_ = 0;  // monotone Nanosekunden, gesetzt in startup()
    // M-B module axis: O(1) index (mod_hash<<32 | sched_id) → row entity of the
    // EcsAppStaModTimComponent upsert in run_schedule. IntMixHash is mandatory
    // for integer keys (ARCH_ASE_HUB_ASYNC 9.7).
    ase::containers::HashMap<uint64_t, Entity, ase::containers::IntMixHash> mod_tim_rows_;
    // Flip-Gedaechtnis des Region-Domain-Gates: -1 unbekannt, 0 laufend, 1 gehalten. Das
    // Gate selbst ist je Durchlauf zustandslos (Anker-Tag + RegionRect-Poolgroesse); dies
    // verhindert nur, dass der Uebergang in jedem Durchlauf erneut geloggt wird.
    int region_hold_state_ = -1;
    std::string current_source_;
    std::string current_version_;
    int boot_delay_us_ = 0;
    int shutdown_delay_us_ = 0;
    void(*on_destroy_callback_)() = nullptr;
    int cli_argc_ = 0;
    char** cli_argv_ = nullptr;
};

// =============================================================================
// SystemBuilder Implementation (inline)
// =============================================================================

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
