#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        boot_logger.hpp
 * @brief       Boot sequence visualization with colored output
 * @description Displays system startup progress with schedule grouping,
 *              module colors, timestamps, and dependency chains. The table is
 *              tiered terminal output: one block per schedule, one line per system.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @usecase     usecase:100960 - write tiered terminal output
 * @actor       Terminal Output Scribe (TERMOUTSCR)
 * @created     2026-02-01
 * @modified    2026-10-30
 * @version     1.1.1
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
// The World every system is started against.
#include <ase/ecs/world.hpp>

namespace ase::ecs::internal {

/**
 * Boot logger configuration.
 */
struct BootLoggerConfig {
    int boot_delay_us = 0;  // Visual delay per system (microseconds), default: 0 (fast)
    bool show_dependencies = true;
    bool show_timestamps = true;
    // The progress table belongs to whoever owns the terminal. A tier server owns it
    // (true, the default); an embedded host such as Godot in the Vivarium client does not
    // and gets no table (false, App with APP_LIFE_HOST). on_start runs in BOTH cases in the
    // same order - only the drawing is suppressed.
    bool render_terminal_table = true;
};

/**
 * Print boot sequence with system on_start calls.
 *
 * Format: a full-width rule of U+2501 (terminal_line), the title line
 * "ASE Schedule Bootstrap (162 systems)", a second rule, then one group per schedule:
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
 * Called by App::startup() when has_pending() is true (late system registration).
 * Calls on_start() on each pending system and logs them in the same boot format.
 *
 * @param registry System registry (systems added during on_start of initial pass)
 * @param world World for on_start calls
 * @param config Logger configuration
 */
void boot_pending_systems(SystemRegistry& registry, World& world,
                          const BootLoggerConfig& config = {});

}  // namespace ase::ecs::internal
