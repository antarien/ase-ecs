#pragma once

/**
 * ASE ECS COMPONENT
 *
 * @file        ecs_app_sta_tim_comp.hpp
 * @brief       Measured schedule execution time (singleton)
 * @description Carries the wall duration of the most recent Dynamics schedule
 *              execution plus a monotonic run counter, so readers can fold each
 *              sample exactly once. Written by App while it drives the tick
 *              loop, read by higher layers for stats and load attribution.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    property/temporal/duration
 * @created     2026-02-01
 * @modified    2026-08-20
 * @version     1.0.0
 *
 * ECS COMPONENT COMPLIANCE
 *
 * [ ] DATA fields ONLY - No methods
 * [ ] NO .cpp file - Header-only
 * [ ] ONLY zero-initialization (= 0, = 0.0f, = false, = {})
 * [ ] No magic numbers in defaults (use types.hpp constants)
 * [ ] Entity references initialized to = 0 (systems set values)
 * [ ] Single responsibility (one data category)
 * [ ] No God-Component (unrelated fields)
 * [ ] Large data in registry.ctx()? (component has only lookup ID!)
 * [ ] Tag structs end with Tag suffix - N/A (not a tag)
 * [ ] Filename: prefix/suffix NOT abbreviated, words between = 3-4 chars
 * [ ] Struct name derived from filename (snake_case to PascalCase)
 * [ ] 1 File = 1 Component
 * [ ] File in correct category subfolder
 * [ ] SHARED components listed in codegen.json components.shared
 * [ ] Pointer components in codegen.json components.server_only
 * [ ] Strings < 64 bytes use char[N] fixed arrays
 * [ ] Strings 64-256 bytes use appropriately sized char[N]
 * [ ] Strings > 256 bytes use registry.ctx() mit Lookup-ID?
 * [ ] NO Entity-per-Character (strings are single attributes, not N-Items!)
 * [ ] Lookup-only strings use uint32_t hash (entt::hashed_string)
 * [ ] NO std::shared_ptr in components (use Flyweight Pattern via ctx!)
 * [ ] NO void* in components (use Flyweight Pattern via ctx!)
 * [ ] NO uint64_t as pointer concept (use uint32_t ID + ResourceManager via ctx!)
 * [ ] External library objects (shared_ptr, handles) in ResourceManager via ctx()
 * [ ] Component stores ONLY primitive ID (uint32_t) referencing external resource
 */

#include <cstdint>

namespace ase::ecs {

/**
 * EcsAppStaTimComponent - Measured schedule execution time (Singleton)
 *
 * App writes this while driving the tick loop (only the orchestrator brackets
 * a schedule execution); higher layers (Kernel stats EMA, load attribution)
 * read it - the seam runs upward only. The downward counterpart that once
 * carried timestep configuration into App was EcsAppStateRatesComponent; it was
 * deleted on 2026-08-20 because TickScheduler derives every rate from
 * schedule_interval() in schedule.hpp, so nothing configures App from above.
 *
 * dynamics_runs is a monotonic sample cursor: readers fold a new
 * dynamics_time_ms sample exactly once by remembering the last seen count.
 *
 * No Hub values: ase-ecs is L1 and ase-hub is an L3 module, so this component
 * cannot be published through the Hub - the readers named above hold it
 * directly.
 */
struct EcsAppStaTimComponent {
    float dynamics_time_ms = 0.0f;  // wall duration of the most recent Dynamics schedule execution (ms)
    uint64_t dynamics_runs = 0;     // monotonic count of Dynamics executions (sample cursor for readers)
};

}  // namespace ase::ecs
