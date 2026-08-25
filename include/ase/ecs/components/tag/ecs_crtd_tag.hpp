#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_crtd_tag.hpp
 * @brief       EcsCrtdTag - Entity created this frame, not yet announced
 * @description Marks an entity that came into existence in the current frame, so that a pass
 *              which announces new entities can find exactly those.
 *              crtd = created
 *
 *              MOVED OUT OF system.hpp ON 2026-08-22 with its three siblings; see
 *              ecs_dstr_tag.hpp for why the module prefix appears only now.
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
 * @brief EcsCrtdTag - The entity is new in this frame
 *
 * State: The entity exists, and no pass that cares about arrivals has processed it yet
 * Filter: registry.view<Component, EcsCrtdTag>() gives exactly the arrivals
 * Added: By whoever creates the entity
 * Removed: By the pass that handled the arrival
 *
 * "New" is a state of one frame, not a property of the entity. It lives as a tag because a
 * boolean field would keep claiming newness after the frame in which it was true.
 */
struct EcsCrtdTag {};

}  // namespace ase::ecs
