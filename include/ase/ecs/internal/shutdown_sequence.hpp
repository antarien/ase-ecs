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
 * Shutdown sequence configuration.
 */
struct ShutdownConfig {
    int shutdown_delay_us = 0;  // Visual delay per system (microseconds), default: 0 (fast)
    bool show_timestamps = true;
    // Wie in BootLoggerConfig: die Tabelle zeichnet nur, wer das Terminal besitzt. Ein
    // eingebetteter Host (App mit APP_LIFE_HOST) setzt false; on_stop laeuft trotzdem fuer
    // jedes System in derselben umgekehrten Reihenfolge.
    bool render_terminal_table = true;
    // Ob der Logger nach dem Abbau seine Senken zurueckbekommt. Ein Tier-Server endet direkt
    // danach mit _exit(0) und spielt die gesammelten Zeilen selbst aufs Terminal (false,
    // Vorgabe). Ein eingebetteter Host LEBT nach dem Abbau weiter: ohne Rueckgabe gingen jede
    // spaetere Logzeile - Entladen der Plugins, Fehler beim Abbau, der naechste Neustart - und
    // die Zeilen aus on_stop selbst ins Leere (true, App mit APP_LIFE_HOST).
    bool restore_log_sinks = false;
    // Gerufen GENAU EINMAL, nachdem das on_stop des LETZTEN Systems zurueckkam - vor Fusszeile und
    // Log-Wiedergabe, also mit noch mitgeschnittenen Zeilen. Der einzige Punkt, an dem kein System
    // mehr laeuft und die App noch steht: dort entlaedt der Kernel seine Module (App::set_on_stopped).
    void (*after_all_stopped)(void* user) = nullptr;
    void* after_all_stopped_user = nullptr;
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
