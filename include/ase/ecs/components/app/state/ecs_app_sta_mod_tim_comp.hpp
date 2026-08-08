#pragma once

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
 */

#include <cstdint>

namespace ase::ecs {

struct EcsAppStaModTimComponent {
    uint32_t mod_hash = 0;  // FNV-1a32 (entt::hashed_string) of the module/plugin source name
    uint32_t grp_id = 0;    // module-group id resolved at registration (mod_grp_of; MOD_GRP_ID_NONE = unregistered)
    uint32_t sched_id = 0;  // ase::ecs::Schedule value these systems ran under (frozen u8 enum values)
    uint64_t time_us = 0;   // running total wall microseconds of this module's systems in this schedule (grows only)
    uint64_t runs = 0;      // running count of completed schedule passes (consumer sample cursor)
};

}  // namespace ase::ecs
