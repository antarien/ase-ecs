/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        tick_scheduler.cpp
 * @brief       Data-driven tick loop using schedule metadata
 * @description Drives every schedule from its own interval and tier instead of
 *              from hardcoded accumulators. schedule_interval() and
 *              schedule_tier() in schedule.hpp are the SSOT; the arrays below
 *              only group the schedules by tier so one loop can walk them.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation/algorithm
 * @created     2026-02-01
 * @modified    2026-08-20
 * @version     1.0.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <ase/ecs/internal/tick_scheduler.hpp>

namespace ase::ecs::internal {

// =============================================================================
// SCHEDULE ARRAYS (grouped by tier)
// =============================================================================

const Schedule FRAME_SCHEDULES[] = {
    Schedule::Reception,
    Schedule::Ingestion,
    Schedule::Integration,
    Schedule::Production,
};

const Schedule KINETIC_SCHEDULES[] = {
    Schedule::Dynamics,
    Schedule::Kinematics,
    Schedule::Collision,
};

const Schedule REACTIVE_SCHEDULES[] = {
    Schedule::Transmission,
    Schedule::Synchronization,
};

const Schedule TACTICAL_SCHEDULES[] = {
    Schedule::Perception,
    Schedule::Reaction,
    Schedule::Navigation,
    Schedule::Evaluation,
};

const Schedule ADAPTIVE_SCHEDULES[] = {
    Schedule::Deliberation,
    Schedule::Aggregation,
    Schedule::Correlation,
    Schedule::Coordination,
};

const Schedule PROGRESSIVE_SCHEDULES[] = {
    Schedule::Modulation,
    Schedule::Regulation,
    Schedule::Adaptation,
};

const Schedule CYCLIC_SCHEDULES[] = {
    Schedule::Dissemination,
    Schedule::Preservation,
    Schedule::Observation,
};

const Schedule GRADUAL_SCHEDULES[] = {
    Schedule::Accumulation,
    Schedule::Consolidation,
};

const Schedule INCREMENTAL_SCHEDULES[] = {
    Schedule::Maintenance,
    Schedule::Reconciliation,
};

const Schedule AMBIENT_SCHEDULES[] = {
    Schedule::Maturation,
    Schedule::Degradation,
};

const Schedule PERIODIC_SCHEDULES[] = {
    Schedule::Regeneration,
    Schedule::Decomposition,
};

const Schedule EPOCHAL_SCHEDULES[] = {
    Schedule::Evolution,
    Schedule::Erosion,
};

const Schedule EXTENDED_SCHEDULES[] = {
    Schedule::Succession,
    Schedule::Transformation,
};

const Schedule DIURNAL_SCHEDULES[] = {
    Schedule::Culmination,
    Schedule::Renewal,
};

// =============================================================================
// TIER CONFIGURATION TABLE
// =============================================================================

static const TierConfig TIER_CONFIGS[TIER_COUNT] = {
    // Frame (~60Hz) - every tick
    {"Frame",       0.0f,       false, FRAME_SCHEDULES,       4},
    // Kinetic (30Hz) - physics with fixed timestep
    {"Kinetic",     1.0f/30.0f, true,  KINETIC_SCHEDULES,     3},
    // Reactive (20Hz) - network sync
    {"Reactive",    1.0f/20.0f, false, REACTIVE_SCHEDULES,    2},
    // Tactical (10Hz) - fast AI
    {"Tactical",    1.0f/10.0f, false, TACTICAL_SCHEDULES,    4},
    // Adaptive (5Hz) - AI planning
    {"Adaptive",    1.0f/5.0f,  false, ADAPTIVE_SCHEDULES,    4},
    // Progressive (2Hz) - slow adjustments
    {"Progressive", 1.0f/2.0f,  false, PROGRESSIVE_SCHEDULES, 3},
    // Cyclic (1Hz) - regular intervals
    {"Cyclic",      1.0f,       false, CYCLIC_SCHEDULES,      3},
    // Gradual (10s) - slow accumulation
    {"Gradual",     10.0f,      false, GRADUAL_SCHEDULES,     2},
    // Incremental (1min) - maintenance
    {"Incremental", 60.0f,      false, INCREMENTAL_SCHEDULES, 2},
    // Ambient (5min) - very slow
    {"Ambient",     300.0f,     false, AMBIENT_SCHEDULES,     2},
    // Periodic (15min) - periodic
    {"Periodic",    900.0f,     false, PERIODIC_SCHEDULES,    2},
    // Epochal (1h) - hourly
    {"Epochal",     3600.0f,    false, EPOCHAL_SCHEDULES,     2},
    // Extended (6h) - quarter-day
    {"Extended",    21600.0f,   false, EXTENDED_SCHEDULES,    2},
    // Diurnal (24h) - daily
    {"Diurnal",     86400.0f,   false, DIURNAL_SCHEDULES,     2},
};

const TierConfig* get_tier_configs() {
    return TIER_CONFIGS;
}

// =============================================================================
// TICK SCHEDULER IMPLEMENTATION
// =============================================================================

TickScheduler::TickScheduler() {
    reset();
}

void TickScheduler::reset() {
    accumulators_.fill(0.0f);
}

void TickScheduler::tick(float dt, ScheduleRunner run, void* user) {
    // Clamp frame time
    if (dt > max_frame_time_) {
        dt = max_frame_time_;
    }

    const TierConfig* configs = get_tier_configs();

    for (size_t tier = 0; tier < TIER_COUNT; ++tier) {
        const auto& config = configs[tier];

        if (config.interval <= 0.0f) {
            // Frame tier: run every tick
            for (size_t i = 0; i < config.schedule_count; ++i) {
                run(user, config.schedules[i], dt);
            }
        } else if (config.fixed_timestep) {
            // Fixed timestep (physics): use while-loop
            accumulators_[tier] += dt;
            while (accumulators_[tier] >= config.interval) {
                for (size_t i = 0; i < config.schedule_count; ++i) {
                    run(user, config.schedules[i], config.interval);
                }
                accumulators_[tier] -= config.interval;
            }
        } else {
            // Variable timestep: run once per interval
            accumulators_[tier] += dt;
            if (accumulators_[tier] >= config.interval) {
                for (size_t i = 0; i < config.schedule_count; ++i) {
                    run(user, config.schedules[i], config.interval);
                }
                accumulators_[tier] -= config.interval;
            }
        }
    }

    // Conclusion always runs at end of frame
    run(user, Schedule::Conclusion, dt);
}

}  // namespace ase::ecs::internal
