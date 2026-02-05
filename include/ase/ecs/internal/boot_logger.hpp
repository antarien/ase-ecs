#pragma once

/**
 * ASE ECS Internal - Boot Logger
 *
 * @file        boot_logger.hpp
 * @brief       Boot sequence visualization with colored output
 * @description Displays system startup progress with schedule grouping,
 *              module colors, timestamps, and dependency chains.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
 */

#include <ase/ecs/internal/system_registry.hpp>
#include <ase/ecs/system.hpp>

namespace ase::ecs::internal {

/**
 * Boot logger configuration.
 */
struct BootLoggerConfig {
    int boot_delay_us = 0;  // Visual delay per system (microseconds), default: 0 (fast)
    bool show_dependencies = true;
    bool show_timestamps = true;
};

/**
 * Print boot sequence with system on_start calls.
 *
 * Format:
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *   ASE Schedule Bootstrap (162 systems)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 *   ┌─ Initialization (once)
 *   │ [46:40.867] [Boot] [001/162] [ase-kernel] [OK] KernelCoreLifecycleSystem
 *   │ [46:40.883] [Boot] [002/162] [ase-mongodb] [OK] MongodbDbConnSystem
 *
 * @param registry System registry with all systems
 * @param world World for on_start calls
 * @param config Logger configuration
 */
void print_boot_sequence(SystemRegistry& registry, World& world,
                         const BootLoggerConfig& config = {});

}  // namespace ase::ecs::internal
