#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_dstr_tag.hpp
 * @brief       EcsDstrTag - Entity marked for deferred deletion
 * @description Marks an entity that a cleanup pass removes at a safe point in the frame.
 *              dstr = destroy
 *
 *              MOVED OUT OF system.hpp ON 2026-08-22, together with its three siblings. It
 *              was declared there as `DestroyTag` since the file was named ecs.hpp; the rule
 *              `struct in System files forbidden` reached it only after the 2026-01-15 rename
 *              moved that file into the system-header rule set. The name gained its module
 *              prefix here because the naming schema derives it from the filename - a
 *              consequence of the destination, not a rename to silence a detector.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    tag
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
 * @brief EcsDstrTag - The entity is scheduled for removal, not yet removed
 *
 * State: The entity still exists and still answers every view it matched before. Only the
 *        cleanup pass acts on the mark.
 * Filter: Systems that must not touch a dying entity exclude it -
 *         registry.view<Component>(entt::exclude<EcsDstrTag>)
 * Added: By whoever decides the entity is finished
 * Removed: Never - the entity carrying it is destroyed
 *
 * WHY A MARK AND NOT A CALL: destroying an entity inside a view iteration invalidates the
 * iterator. The tag turns "delete now" into "delete at a safe point", which is the whole
 * reason deferred deletion exists.
 */
struct EcsDstrTag {};

}  // namespace ase::ecs
