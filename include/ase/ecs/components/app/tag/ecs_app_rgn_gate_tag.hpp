#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_app_rgn_gate_tag.hpp
 * @brief       EcsAppRgnGateTag - This process runs region-bound simulation and carries the gate
 * @description Marks that App::run_schedule shall arm the region-domain gate: systems whose
 *              module plane is SIMULATION (the default plane of every module without a
 *              declared exception) tick ONLY while the process owns at least one
 *              types::RegionRect seam row. The WORLD kernel stamps this mark once at tier
 *              initialization - the compute tier is the only tier whose module population is
 *              region-bound simulation (operator ruling 2026-08-26: "bis auf wenige
 *              uebernimmt jedes Modul Simulationsaufgaben in World").
 *
 *              WHY A TAG AND NOT A TIER ENUM IN L1: the App never knows tiers - that is a
 *              statement, not an oversight (same shape as EcsAppQuitReqTag). The PRODUCER of
 *              the arming decision is the kernel that knows what it was built as; the
 *              CONSUMER is App::run_schedule. On every tier that never stamps this mark the
 *              gate stays cold and the loop is byte-identical to before.
 *
 *              NOBODY REMOVES IT. Arming is a property of the process build, not of a phase;
 *              the gate OPENS and CLOSES with the RegionRect coverage, never with this mark.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    tag
 * @parity      server_only
 * @created     2026-08-26
 * @modified    2026-08-26
 * @version     1.0.0
 * @author      Jan Ohlmann (ADG/ASE/AOW)
 *
 * ECS TAG COMPLIANCE
 *
 * [ ] DATA fields ONLY - No methods (empty struct for tags)
 * [ ] NO .cpp file - Header-only
 * [ ] ONLY zero-initialization - N/A (no fields)
 * [ ] No magic numbers in defaults - N/A (no fields)
 * [ ] Entity references - N/A (no fields)
 * [ ] Single responsibility - N/A (marker only)
 * [ ] No God-Component - N/A (no fields)
 * [ ] Large data uses pointer pattern - N/A (no data)
 * [ ] Large data in registry.ctx() - N/A (Tags have no data)
 * [ ] Tag structs end with Tag suffix
 * [ ] Filename: prefix/suffix NOT abbreviated, words between = 3-4 chars
 * [ ] Struct name: Remove tag_ from middle, add Tag suffix
 * [ ] 1 File = 1 Component
 * [ ] File in tag/ subfolder (with optional deeper hierarchy)
 * [ ] Per-entity runtime values use state/ components (NOT types.hpp!)
 * [ ] SHARED components listed in codegen.json components.shared
 * [ ] Pointer components in codegen.json components.server_only
 * [ ] Tag replaces `bool is_*` or `bool has_*` field in Component
 * [ ] Tag replaces `uint8_t *_type` field with if-chain dispatch
 * [ ] Systems use View filter instead of if-else inside loop
 * [ ] INCLUDE: registry.view<Component, ThisTag>()
 * [ ] EXCLUDE: registry.view<Component>(entt::exclude<ThisTag>)
 * [ ] NO if (entity.has<Tag>) inside loop - use filtered View!
 * [ ] NO switch/case on type - use separate View per Tag!
 * [ ] Each state = separate Tag + separate View in System
 * [ ] N-item support via Entity-per-Item + Tags
 */

namespace ase::ecs {

/**
 * @brief EcsAppRgnGateTag - The region-domain gate of App::run_schedule is armed
 *
 * State: The kernel of a region-bound compute build (today: Tier::World) has declared
 *        that SIMULATION-plane systems tick only under RegionRect coverage
 * Filter: App::run_schedule takes the storage size of this tag and arms the gate on non-empty
 * Added: By the tier-knowing producer (KernelTierIniSystem of the World build) - never by App
 * Removed: Never - the gate opens and closes with coverage, not with this mark
 */
struct EcsAppRgnGateTag {};

}  // namespace ase::ecs
