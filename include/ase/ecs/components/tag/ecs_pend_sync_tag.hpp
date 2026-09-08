#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_pend_sync_tag.hpp
 * @brief       EcsPendSyncTag - Entity waiting for its synchronisation pass
 * @description Marks an entity whose state is queued for synchronisation and has not been
 *              carried across yet.
 *              pend = pending, sync = sync
 *
 *              MOVED OUT OF system.hpp ON 2026-08-22 with its three siblings; see
 *              ecs_dstr_tag.hpp for why the module prefix appears only now.
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
 * @brief EcsPendSyncTag - Queued for synchronisation, not yet carried across
 *
 * State: The entity is registered for a sync pass that has not run for it
 * Filter: registry.view<Component, EcsPendSyncTag>() is the queue
 * Added: By whoever enqueues the entity for synchronisation
 * Removed: By the sync pass once the state is across
 *
 * SEPARATE FROM EcsDtyTag ON PURPOSE: dirty says the value MOVED, pending-sync says a transfer
 * is OWED. An entity can be dirty without anyone owing a transfer, and it can owe one after a
 * failed attempt without having changed again. Folding the two would lose exactly that
 * difference, and the loss would show up as silently skipped transfers.
 */
struct EcsPendSyncTag {};

}  // namespace ase::ecs
