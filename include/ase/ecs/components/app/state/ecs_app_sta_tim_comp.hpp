#pragma once

/**
 * EcsAppStaTimComponent - Measured schedule execution time (Singleton)
 *
 * App writes this while driving the tick loop (only the orchestrator brackets
 * a schedule execution); higher layers (Kernel stats EMA, load attribution)
 * read it. Mirror seam of EcsAppStateRatesComponent, opposite direction.
 *
 * dynamics_runs is a monotonic sample cursor: readers fold a new
 * dynamics_time_ms sample exactly once by remembering the last seen count.
 */

#include <cstdint>

namespace ase::ecs {

struct EcsAppStaTimComponent {
    float dynamics_time_ms = 0.0f;  // wall duration of the most recent Dynamics schedule execution (ms)
    uint64_t dynamics_runs = 0;     // monotonic count of Dynamics executions (sample cursor for readers)
};

}  // namespace ase::ecs
