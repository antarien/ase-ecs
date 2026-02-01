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
#include "system.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace ase::ecs {

// =============================================================================
// Version Detection Helper
// =============================================================================

namespace detail {

// SFINAE helper to detect if T has a static version() method
template<typename T, typename = void>
struct has_version : std::false_type {};

template<typename T>
struct has_version<T, std::void_t<decltype(T::version())>> : std::true_type {};

template<typename T>
inline constexpr bool has_version_v = has_version<T>::value;

// Get version if available, otherwise return empty string
template<typename T>
const char* get_version() {
    if constexpr (has_version_v<T>) {
        return T::version();
    } else {
        return "";
    }
}

}  // namespace detail

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
    std::vector<std::string> after_;
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
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;
    using TimePoint = Clock::time_point;

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
     * Add the kernel (Layer 2 - Foundation).
     * Must be called first, before modules and plugins.
     */
    template<Kernel K>
    App& add_kernel() {
        K kernel;
        current_source_ = K::name();
        current_version_ = detail::get_version<K>();
        kernel.build(*this);
        current_source_.clear();
        current_version_.clear();
        return *this;
    }

    /**
     * Add a plugin (Layer 4 - Optional features).
     */
    template<Plugin P>
    App& add_plugin() {
        P plugin;
        current_source_ = P::name();
        current_version_ = detail::get_version<P>();
        plugin.build(*this);
        current_source_.clear();
        current_version_.clear();
        return *this;
    }

    template<Plugin P>
    App& add_plugin(P&& plugin) {
        current_source_ = std::decay_t<P>::name();
        current_version_ = detail::get_version<std::decay_t<P>>();
        plugin.build(*this);
        current_source_.clear();
        current_version_.clear();
        return *this;
    }

    /**
     * Add a module (Layer 3 - Core game systems).
     */
    template<Module M>
    App& add_module() {
        M module;
        current_source_ = M::name();
        current_version_ = detail::get_version<M>();
        module.build(*this);
        current_source_.clear();
        current_version_.clear();
        return *this;
    }

    template<Module M>
    App& add_module(M&& module) {
        current_source_ = std::decay_t<M>::name();
        current_version_ = detail::get_version<std::decay_t<M>>();
        module.build(*this);
        current_source_.clear();
        current_version_.clear();
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
                         std::vector<std::string> after, int priority,
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
    World world_;

    // Internal components (PIMPL for clean separation)
    std::unique_ptr<internal::TickScheduler> tick_scheduler_;
    std::unique_ptr<internal::SystemRegistry> system_registry_;

    std::atomic<bool> running_{false};
    TimePoint last_frame_time_;
    std::string current_source_;
    std::string current_version_;
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
