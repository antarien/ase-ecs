#pragma once

/**
 * ASE ECS App - Bevy-Style Application Builder
 *
 * Usage:
 *   ecs::App()
 *       .add_plugin<KernelPlugin>()
 *       .add_plugin<PlayerPlugin>()
 *       .add_system<MySystem>(Schedule::Update)
 *       .run();
 */

#include "schedule.hpp"
#include "ecs.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace ase::ecs {

// Forward declarations
class App;

// =============================================================================
// Module Concept (Layer 3) - Core game systems, statically linked
// =============================================================================

template<typename T>
concept Module = requires(T module, App& app) {
    { module.build(app) } -> std::same_as<void>;
};

// =============================================================================
// Plugin Concept (Layer 4) - Optional, hot-loadable features
// =============================================================================

template<typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
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
    App(App&&) = default;
    App& operator=(App&&) = default;

    // =========================================================================
    // Builder API
    // =========================================================================

    /**
     * Add a plugin. Plugin::build(App&) is called immediately.
     */
    template<Plugin P>
    App& add_plugin() {
        P plugin;
        plugin.build(*this);
        return *this;
    }

    /**
     * Add a plugin instance.
     */
    template<Plugin P>
    App& add_plugin(P&& plugin) {
        plugin.build(*this);
        return *this;
    }

    /**
     * Add a module (Layer 3 - Core game systems).
     */
    template<Module M>
    App& add_module() {
        M module;
        module.build(*this);
        return *this;
    }

    /**
     * Add a module instance.
     */
    template<Module M>
    App& add_module(M&& module) {
        module.build(*this);
        return *this;
    }

    /**
     * Add a system to a schedule.
     */
    template<typename S>
    App& add_system(Schedule schedule) {
        auto system = std::make_unique<S>();
        schedule_systems_[schedule].push_back(std::move(system));
        return *this;
    }

    /**
     * Add a system instance to a schedule.
     */
    App& add_system(Schedule schedule, std::unique_ptr<System> system) {
        schedule_systems_[schedule].push_back(std::move(system));
        return *this;
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * Run the application (blocking main loop).
     */
    void run();

    /**
     * Request application exit.
     */
    void quit() { running_.store(false); }

    /**
     * Check if running.
     */
    bool is_running() const { return running_.load(); }

    // =========================================================================
    // World Access
    // =========================================================================

    World& world() { return world_; }
    const World& world() const { return world_; }

    Registry& registry() { return world_.registry(); }
    const Registry& registry() const { return world_.registry(); }

private:
    void run_schedule(Schedule schedule, float dt);

    World world_;
    std::unordered_map<Schedule, std::vector<std::unique_ptr<System>>> schedule_systems_;

    // Timestep accumulators
    float fixed_accumulator_ = 0.0f;
    float replication_accumulator_ = 0.0f;
    float persistence_accumulator_ = 0.0f;

    // Rates (can be configured via components)
    float fixed_dt_ = 1.0f / 30.0f;
    float replication_dt_ = 1.0f / 20.0f;
    float persistence_dt_ = 1.0f;
    float max_frame_time_ = 0.25f;

    std::atomic<bool> running_{false};
    TimePoint last_frame_time_;
};

}  // namespace ase::ecs
