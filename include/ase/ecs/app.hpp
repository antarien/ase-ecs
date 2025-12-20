#pragma once

/**
 * ASE ECS App - Bevy-Style Application Builder
 *
 * Usage:
 *   ecs::App()
 *       .add_kernel<ase::kernel::Kernel>()
 *       .add_module<PlayerModule>()
 *       .add_plugin<SkyPlugin>()
 *       .run();
 */

#include "schedule.hpp"
#include "ecs.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace ase::ecs {

// Forward declarations
class App;
class SystemBuilder;

// =============================================================================
// SystemInfo - Metadata for boot log
// =============================================================================

struct SystemInfo {
    std::string name;
    std::string source;  // Module/Plugin name (e.g., "ase-network", "ase-pl-sky")
    Schedule schedule = Schedule::Update;
    std::vector<std::string> run_after;
    int priority = 0;
};

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
    std::vector<std::string> after_;
    std::string source_;
    int priority_ = 0;
    bool finalized_ = false;
};

// =============================================================================
// App - Bevy-Style Application Builder
// =============================================================================

class App {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;
    using TimePoint = Clock::time_point;

    App();
    ~App() = default;

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
     * Add the kernel (Layer 2 - Foundation).
     * Must be called first, before modules and plugins.
     */
    template<Kernel K>
    App& add_kernel() {
        K kernel;
        current_source_ = K::name();
        kernel.build(*this);
        current_source_.clear();
        return *this;
    }

    /**
     * Add a plugin (Layer 4 - Optional features).
     */
    template<Plugin P>
    App& add_plugin() {
        P plugin;
        current_source_ = P::name();
        plugin.build(*this);
        current_source_.clear();
        return *this;
    }

    template<Plugin P>
    App& add_plugin(P&& plugin) {
        current_source_ = P::name();
        plugin.build(*this);
        current_source_.clear();
        return *this;
    }

    /**
     * Add a module (Layer 3 - Core game systems).
     */
    template<Module M>
    App& add_module() {
        M module;
        current_source_ = M::name();
        module.build(*this);
        current_source_.clear();
        return *this;
    }

    template<Module M>
    App& add_module(M&& module) {
        current_source_ = M::name();
        module.build(*this);
        current_source_.clear();
        return *this;
    }

    /**
     * Add a system to a schedule (simple version).
     */
    template<typename S>
    App& add_system(Schedule schedule) {
        auto system = std::make_unique<S>();
        SystemInfo info{
            .name = system->name(),
            .source = current_source_,
            .schedule = schedule,
            .run_after = {},
            .priority = 0
        };
        system_infos_.push_back(std::move(info));
        schedule_systems_[schedule].push_back(std::move(system));
        return *this;
    }

    /**
     * Add a system with builder for dependencies.
     * Usage: app.add_system_with<MySystem>(Schedule::Update).run_after("OtherSystem").done();
     */
    template<typename S>
    SystemBuilder add_system_with(Schedule schedule) {
        auto system = std::make_unique<S>();
        return SystemBuilder(*this, schedule, std::move(system));
    }

    /**
     * Internal: Called by SystemBuilder to finalize system addition.
     */
    void finalize_system(Schedule schedule, std::unique_ptr<System> system,
                         std::vector<std::string> after, int priority,
                         std::string source = {});

    const std::string& current_source() const { return current_source_; }

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
     */
    void run_schedule(Schedule schedule, float dt);

    void quit() { running_.store(false); }
    bool is_running() const { return running_.load(); }

    // =========================================================================
    // World Access
    // =========================================================================

    World& world() { return world_; }
    const World& world() const { return world_; }
    Registry& registry() { return world_.registry(); }
    const Registry& registry() const { return world_.registry(); }

private:
    void print_boot_log();
    void sort_systems_by_dependencies();

    World world_;
    std::unordered_map<Schedule, std::vector<std::unique_ptr<System>>> schedule_systems_;
    std::vector<SystemInfo> system_infos_;

    float fixed_accumulator_ = 0.0f;
    float replication_accumulator_ = 0.0f;
    float persistence_accumulator_ = 0.0f;
    float fixed_dt_ = 1.0f / 30.0f;
    float replication_dt_ = 1.0f / 20.0f;
    float persistence_dt_ = 1.0f;
    float max_frame_time_ = 0.25f;

    std::atomic<bool> running_{false};
    TimePoint last_frame_time_;
    std::string current_source_;
};

// =============================================================================
// SystemBuilder Implementation (inline)
// =============================================================================

inline SystemBuilder::SystemBuilder(App& app, Schedule schedule, std::unique_ptr<System> system)
    : app_(app), schedule_(schedule), system_(std::move(system)), source_(app.current_source()) {}

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
        app_.finalize_system(schedule_, std::move(system_), std::move(after_), priority_, std::move(source_));
        finalized_ = true;
    }
    return app_;
}

inline SystemBuilder::~SystemBuilder() {
    if (!finalized_ && system_) {
        app_.finalize_system(schedule_, std::move(system_), std::move(after_), priority_, std::move(source_));
    }
}

}  // namespace ase::ecs
