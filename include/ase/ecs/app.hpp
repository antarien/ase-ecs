#pragma once

/**
 * ASE ECS App - Bevy-Style Application Runner
 *
 * The App owns the World and runs the main loop.
 * Timestep configuration is read from Kernel-Components AFTER Startup.
 *
 * Usage:
 *   ecs::App app;
 *   app.run();  // Systems registered via REGISTER_SYSTEM
 */

#include "schedule.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace ase::ecs {

// Forward declarations
class World;

// =============================================================================
// Timestep Configuration
// =============================================================================

struct TimestepConfig {
    float rate_hz = 0.0f;
    float accumulator = 0.0f;
    float dt = 0.0f;
};

// =============================================================================
// App - The Main Application
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

    World& world() { return *world_; }
    const World& world() const { return *world_; }

private:
    void frame(float dt);
    void read_kernel_config();
    bool should_run_schedule(Schedule schedule, float frame_dt);

    std::unique_ptr<World> world_;
    std::unordered_map<Schedule, TimestepConfig> timesteps_;
    float max_frame_time_ = 0.25f;
    std::atomic<bool> running_{false};
    TimePoint last_frame_time_;
};

}  // namespace ase::ecs
