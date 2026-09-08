#pragma once

/**
 * ASE ECS COMPONENT (TAG)
 *
 * @file        ecs_app_quit_req_tag.hpp
 * @brief       EcsAppQuitReqTag - A layer above L1 asks the process loop to end
 * @description Marks that the application run loop shall stop after the current tick.
 *              App::tick() polls this mark every frame and clears the running flag when it
 *              exists - the ECS form of App::quit(), reachable from ANY layer that can write
 *              the registry, without handing anybody the App.
 *
 *              WHY A TAG AND NOT A CALL: a System's tick() receives the Registry and never
 *              the App - that is a statement, not an oversight, and it is why no system can
 *              end the process by itself. Before this mark existed, exactly ONE server
 *              (ase-server-reasoning) bridged kernel-stop to App::quit() in its own main.cpp
 *              loop; the four other tiers kept ticking a stopped kernel forever. The bridge
 *              belongs to the PRODUCER of the stop (KernelCoreLfcSystem stamps this mark in
 *              the same pass that removes KernelCoreRunnTag), and the CONSUMER is App::tick()
 *              itself - the same shape as the signalfd shutdown watch: the quit is an EVENT
 *              the tick collects.
 *
 *              NOBODY REMOVES IT. Its presence ends the process; a removal path would be a
 *              claim that a half-stopped App can be re-armed, and no such path exists.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    tag
 * @parity      server_only
 * @created     2026-08-25
 * @modified    2026-08-25
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
 * @brief EcsAppQuitReqTag - The process run loop shall end after the current tick
 *
 * State: Some layer above L1 has declared the process done (today: the kernel, in the
 *        same KernelCoreLfcSystem pass that removes KernelCoreRunnTag)
 * Filter: App::tick() takes registry.view<EcsAppQuitReqTag>() and stops on non-empty
 * Added: By the producer of the stop decision - never by App itself
 * Removed: Never - presence is terminal, App::shutdown() tears the registry down with it
 */
struct EcsAppQuitReqTag {};

}  // namespace ase::ecs
