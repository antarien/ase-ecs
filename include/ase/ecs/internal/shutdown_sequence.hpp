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
 * @created     2026-02-01
 * @modified    2026-08-20
 * @version     1.0.0
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
