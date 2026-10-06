#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        boot_logger.hpp
 * @brief       Boot sequence visualization with colored output
 * @description Displays system startup progress with schedule grouping,
 *              module colors, timestamps, and dependency chains.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-02-01
 * @modified    2026-10-05
 * @version     1.1.0
 *
 * CORE INFRASTRUCTURE COMPLIANCE
 *
 * [ ] NOT an ECS Component or System
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] No global mutable state (constexpr/const only)
 * [ ] No singletons or static mutable variables
 * [ ] Thread-safe by design (pure functions or explicit mutex)
 * [ ] All public functions documented with @brief, @param, @return
 * [ ] constexpr where possible (compile-time evaluation)
 * [ ] noexcept where possible (no-throw guarantee)
 * [ ] [[nodiscard]] on functions returning values
 * [ ] No magic numbers (use named constants)
 * [ ] No implicit conversions (use explicit constructors)
 * [ ] Header-only OR header+cpp pattern (not mixed)
 * [ ] Include guards via #pragma once
 * [ ] Namespace matches module: ase::{module}
 * [ ] No circular dependencies
 * [ ] No macros (except include guards) - use constexpr/templates
 * [ ] API stable (changes require version bump)
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
    // Die Fortschrittstabelle gehoert dem, der das Terminal besitzt. Ein Tier-Server besitzt
    // es (true, Vorgabe); ein eingebetteter Host wie Godot im Vivarium-Client besitzt es nicht
    // und bekommt keine Tabelle (false, App mit APP_LIFE_HOST). on_start laeuft in BEIDEN
    // Faellen in derselben Reihenfolge - unterdrueckt wird nur das Zeichnen.
    bool render_terminal_table = true;
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
 *   │ [46:40.867] [Boot] [001/162] [ase-kernel] [OK] KernelCoreLfcSystem
 *   │ [46:40.883] [Boot] [002/162] [ase-mongodb] [OK] MongodbDbConnSystem
 *
 * @param registry System registry with all systems
 * @param world World for on_start calls
 * @param config Logger configuration
 */
void print_boot_sequence(SystemRegistry& registry, World& world,
                         const BootLoggerConfig& config = {});

/**
 * Boot only systems that were added after the initial boot pass.
 * Called by App::startup() when has_pending() is true (Late-System-Registration).
 * Calls on_start() on each pending system and logs them in the same boot format.
 *
 * @param registry System registry (systems added during on_start of initial pass)
 * @param world World for on_start calls
 * @param config Logger configuration
 */
void boot_pending_systems(SystemRegistry& registry, World& world,
                          const BootLoggerConfig& config = {});

}  // namespace ase::ecs::internal
