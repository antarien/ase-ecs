#pragma once

/**
 * ASE ECS COMPONENT
 *
 * @file        ecs_app_sta_mod_tim_comp.hpp
 * @brief       Per-(module, schedule) system time row (Entity-per-Item)
 *              declared as running totals that only ever grow, so consumers
 *              difference them against their own cursors instead of resetting
 *              a shared value. One row exists per (module, schedule) pair that
 *              has run at least once.
 * @description Carries the accumulated wall time of one module's systems inside
 *              one schedule, together with the module hash, its resolved group
 *              and the schedule id. App fills it while bracketing System::tick().
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    property/temporal/duration
 * @parity      server_only
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
 * EcsAppStaModTimComponent - per-(module, schedule) system time row (Entity-per-Item)
 *
 * The M-B carrier of the module axis (PLAN_ASE_COMPUTE_MOD_AXIS.md T3): App
 * brackets every System::tick() while driving a schedule and rolls the wall
 * time up per source module - System→Module is known at registration time
 * (SystemInfo.source), the module group resolves once via mod_grp_of
 * (region_wire.hpp registry). One row per (module, schedule) that ever ran.
 *
 * Counter discipline (WorldCchRgnDltComponent precedent): time_us and runs are
 * running totals the writer only ever grows; consumers difference them against
 * their own cursors, so no float drift and every sample folds exactly once.
 *
 * No Hub values: ase-ecs is L1 and ase-hub is an L3 module, so these rows
 * cannot be published through the Hub - consumers read them from the registry.
 */
struct EcsAppStaModTimComponent {
    uint32_t mod_hash = 0;  // FNV-1a32 (entt::hashed_string) of the module/plugin source name
    uint32_t grp_id = 0;    // module-group id resolved at registration (mod_grp_of; MOD_GRP_ID_NONE = unregistered)
    uint32_t sched_id = 0;  // ase::ecs::Schedule value these systems ran under (frozen u8 enum values)
    uint64_t time_us = 0;   // running total wall microseconds of this module's systems in this schedule (grows only)
    uint64_t runs = 0;      // running count of completed schedule passes (consumer sample cursor)
};

}  // namespace ase::ecs
