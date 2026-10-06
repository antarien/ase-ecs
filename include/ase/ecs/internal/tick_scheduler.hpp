#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        tick_scheduler.hpp
 * @brief       Data-driven tick loop using schedule metadata
 * @description Replaces 13 hardcoded accumulators with a single data-driven loop.
 *              Uses schedule_interval() and schedule_tier() from schedule.hpp as SSOT.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation/algorithm
 * @created     2026-02-01
 * @modified    2026-10-06
 * @version     1.1.0
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

#include <ase/ecs/schedule.hpp>

#include <ase/containers/array.hpp>

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
 *
 * @param user     opaque pointer handed back unchanged — the caller's own object
 * @param schedule The schedule to run
 * @param dt       Delta time for this schedule
 *
 * A PLAIN FUNCTION POINTER plus a user pointer, not a std::function. The house form for
 * callbacks in this tree is exactly this pair (see TuiLogCallback in ase-log), and it is not a
 * stylistic preference: a std::function erases the callee's type, may allocate, and calls
 * through an indirection the optimiser cannot see past — inside a tick loop that fires up to
 * 66 times per frame.
 *
 * The user pointer is what makes the plain pointer sufficient. The one caller needs its own
 * object inside the callback, which a capture-less lambda cannot carry; it hands it in here
 * instead, and gets it back at the front of every call.
 */
using ScheduleRunner = void (*)(void* user, Schedule schedule, float dt);

/**
 * Data-driven tick scheduler.
 * Manages accumulators for each tier and calls schedules at correct intervals.
 */
class TickScheduler {
public:
    TickScheduler();

    /**
     * Process one frame tick.
     * @param dt   Frame delta time
     * @param run  Callback to execute schedules
     * @param user opaque pointer passed through to every invocation of run
     */
    void tick(float dt, ScheduleRunner run, void* user);

    /**
     * Reset all accumulators.
     */
    void reset();

    /**
     * Set maximum frame time (clamped if exceeded).
     */
    void set_max_frame_time(float max_dt) { max_frame_time_ = max_dt; }

    /**
     * How often this scheduler ran one schedule since construction or the last reset().
     *
     * Counted where the tier runs, never derived from time: every schedule of a tier runs
     * exactly when its tier runs, so the count of the tier is the count of each of its
     * schedules, and a fixed-timestep tier that catches up runs - and counts - several times in
     * one tick. Conclusion runs once at the end of every tick.
     *
     * @param schedule the schedule to look up
     * @return the number of runs; 0 for a schedule this scheduler does not drive (lifecycle
     *         schedules, game-time schedules)
     */
    [[nodiscard]] uint64_t runs(Schedule schedule) const noexcept;

private:
    ase::containers::Array<float, TIER_COUNT> accumulators_;
    ase::containers::Array<uint64_t, TIER_COUNT> tier_runs_;  // runs of each tier, index as TIER_CONFIGS
    uint64_t tick_count_ = 0;                                  // ticks, and with them runs of Conclusion
    float max_frame_time_ = 0.25f;
};

}  // namespace ase::ecs::internal
