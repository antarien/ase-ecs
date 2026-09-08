#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_dty_tag.hpp
 * @brief       EcsDtyTag - Entity changed since the last broadcast
 * @description Marks an entity whose state a replication or broadcast pass still has to ship.
 *              dty = dirty
 *
 *              THE ABBREVIATION IS dty, NOT drty. The taxonomy catalogue maps `dirty` to
 *              `dty`; `drty` also appears in the catalogue, but for a different word. A probe
 *              that asks "does drty exist" confirms any guess - the direction that decides is
 *              WORD to ABBREVIATION.
 *
 *              MOVED OUT OF system.hpp ON 2026-08-22 with its three siblings; see ecs_dstr_tag.hpp
 *              for why the module prefix appears only now.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    tag
 * @parity      server_only
 * @created     2026-08-22
 * @modified    2026-08-22
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
 * @brief EcsDtyTag - This entity's state has moved and has not been shipped yet
 *
 * State: The entity holds a value a consumer downstream has not seen
 * Filter: A broadcast pass takes registry.view<Component, EcsDtyTag>() and ships exactly those
 * Added: By whoever writes the value
 * Removed: By the pass that shipped it - the mark is the queue, its absence is the receipt
 *
 * THE TAG IS THE ONLY ONE OF THE FOUR WITH CONSUMERS OUTSIDE ase-ecs, measured 2026-08-22:
 * four files bind it qualified, among them sky_cel_star_sys.cpp with three
 * emplace_or_replace calls. A raw word search finds eight - the other four are comments and
 * modules carrying their OWN dirty tag (network_core_drty_tag.hpp, player_drty_tag.hpp).
 * Those are different types with a similar name; do not fold them into this one.
 */
struct EcsDtyTag {};

}  // namespace ase::ecs
