#pragma once

/**
 * ASE ECS Internal - Tick Scheduler
 *
 * @file        tick_scheduler.hpp
 * @brief       Data-driven tick loop using schedule metadata
 * @description Replaces 13 hardcoded accumulators with a single data-driven loop.
 *              Uses schedule_interval() and schedule_tier() from schedule.hpp as SSOT.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
 */

#include <ase/ecs/schedule.hpp>

#include <array>
#include <functional>

namespace ase::ecs::internal {

// =============================================================================
// TIER CONFIGURATION (derived from schedule.hpp SSOT)
// =============================================================================

/**
 * Tier configuration for tick scheduling.
 * Each tier groups schedules that run at the same frequency.
 */
struct TierConfig {
    const char* name;           // Tier name (e.g., "Frame", "Kinetic")
    float interval;             // Interval in seconds (0 = every frame)
    bool fixed_timestep;        // Use while-loop for fixed timestep (physics)
    const Schedule* schedules;  // Array of schedules in this tier
    size_t schedule_count;      // Number of schedules
};

// Schedule arrays for each tier (defined in cpp)
extern const Schedule FRAME_SCHEDULES[];
extern const Schedule KINETIC_SCHEDULES[];
extern const Schedule REACTIVE_SCHEDULES[];
extern const Schedule TACTICAL_SCHEDULES[];
extern const Schedule ADAPTIVE_SCHEDULES[];
extern const Schedule PROGRESSIVE_SCHEDULES[];
extern const Schedule CYCLIC_SCHEDULES[];
extern const Schedule GRADUAL_SCHEDULES[];
extern const Schedule INCREMENTAL_SCHEDULES[];
extern const Schedule AMBIENT_SCHEDULES[];
extern const Schedule PERIODIC_SCHEDULES[];
extern const Schedule EPOCHAL_SCHEDULES[];
extern const Schedule EXTENDED_SCHEDULES[];
extern const Schedule DIURNAL_SCHEDULES[];

// Number of tiers
constexpr size_t TIER_COUNT = 14;

// Get tier configurations (defined in cpp)
const TierConfig* get_tier_configs();

// =============================================================================
// TICK SCHEDULER
// =============================================================================

/**
 * Callback type for running a schedule.
 * @param schedule The schedule to run
 * @param dt Delta time for this schedule
 */
using ScheduleRunner = std::function<void(Schedule, float)>;

/**
 * Data-driven tick scheduler.
 * Manages accumulators for each tier and calls schedules at correct intervals.
 */
class TickScheduler {
public:
    TickScheduler();

    /**
     * Process one frame tick.
     * @param dt Frame delta time
     * @param run Callback to execute schedules
     */
    void tick(float dt, ScheduleRunner run);

    /**
     * Reset all accumulators.
     */
    void reset();

    /**
     * Set maximum frame time (clamped if exceeded).
     */
    void set_max_frame_time(float max_dt) { max_frame_time_ = max_dt; }

private:
    std::array<float, TIER_COUNT> accumulators_;
    float max_frame_time_ = 0.25f;
};

}  // namespace ase::ecs::internal
