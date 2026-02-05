#pragma once

/**
 * ASE ECS Internal - Shutdown Sequence
 *
 * @file        shutdown_sequence.hpp
 * @brief       Shutdown sequence visualization with colored output
 * @description Displays system shutdown progress in reverse order,
 *              with module grouping, colors, and timestamps.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
 */

#include <ase/ecs/internal/system_registry.hpp>
#include <ase/ecs/system.hpp>

namespace ase::ecs::internal {

/**
 * Shutdown sequence configuration.
 */
struct ShutdownConfig {
    int shutdown_delay_us = 0;  // Visual delay per system (microseconds), default: 0 (fast)
    bool show_timestamps = true;
};

/**
 * Print shutdown sequence with system on_stop calls.
 *
 * Format:
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *   ASE Shutdown Sequence (162 systems)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 *   ┌─ Shutdown (once)
 *   │ [46:40.867] [Down] [162/162] [ase-pl-sky] [OK] SkyRenderSyncSystem
 *   │ [46:40.883] [Down] [161/162] [ase-pl-sky] [OK] SkyTimeSystem
 *
 * @param registry System registry with all systems
 * @param world World for on_stop calls
 * @param config Shutdown configuration
 */
void print_shutdown_sequence(SystemRegistry& registry, World& world,
                              const ShutdownConfig& config = {});

}  // namespace ase::ecs::internal
