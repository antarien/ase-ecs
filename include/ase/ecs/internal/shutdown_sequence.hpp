#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        shutdown_sequence.hpp
 * @brief       Shutdown sequence visualization with colored output
 * @description Displays system shutdown progress in reverse order,
 *              with module grouping, colors, and timestamps.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @usecase     usecase:100356 - run the shutdown sequence
 * @actor       Entity Registry Steward (ENTIREGSTE)
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
// The World every system is stopped against.
#include <ase/ecs/world.hpp>

namespace ase::ecs::internal {

/**
 * Shutdown sequence configuration.
 */
struct ShutdownConfig {
    int shutdown_delay_us = 0;  // Visual delay per system (microseconds), default: 0 (fast)
    bool show_timestamps = true;
    // As in BootLoggerConfig: only whoever owns the terminal draws the table. An embedded
    // host (App with APP_LIFE_HOST) sets false; on_stop still runs for every system in the
    // same reverse order.
    bool render_terminal_table = true;
    // Whether the logger gets its sinks back after the teardown. A tier server ends right
    // after with _exit(0) and plays the collected lines onto the terminal itself (false, the
    // default). An embedded host LIVES ON after the teardown: without the return, every later
    // log line - unloading the plugins, teardown errors, the next restart - and the lines
    // from on_stop itself would go nowhere (true, App with APP_LIFE_HOST).
    bool restore_log_sinks = false;
    // Called EXACTLY ONCE after on_stop of the LAST system returned - before the footer and
    // the log replay, so with lines still captured. The one point where no system runs any
    // more and the App still stands: the kernel unloads its modules there (App::set_on_stopped).
    void (*after_all_stopped)(void* user) = nullptr;
    void* after_all_stopped_user = nullptr;
};

/**
 * Print shutdown sequence with system on_stop calls.
 *
 * Format: a full-width rule of U+2501 (terminal_line), the title line
 * "ASE Shutdown Sequence (162 systems)", a second rule, then the systems in reverse order:
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
