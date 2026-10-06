#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        terminal_utils.hpp
 * @brief       ANSI colors, timestamps, terminal width detection
 * @description Shared utilities for boot/shutdown visualization.
 *              Internal implementation detail of ase-ecs.
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

#include <string>

namespace ase::ecs::internal {

// =============================================================================
// ANSI COLOR CONSTANTS
// =============================================================================

namespace ansi {

constexpr const char* RESET   = "\x1b[0m";
constexpr const char* DIM     = "\x1b[38;5;243m";
constexpr const char* CYAN    = "\x1b[36m";
constexpr const char* GREEN   = "\x1b[32m";
constexpr const char* YELLOW  = "\x1b[33m";
constexpr const char* MAGENTA = "\x1b[35m";
constexpr const char* BLUE    = "\x1b[34m";
constexpr const char* RED     = "\x1b[31m";
constexpr const char* WHITE   = "\x1b[37m";

// Muted green (same as [INF] in logs)
constexpr const char* OK_GREEN = "\x1b[38;5;71m";

}  // namespace ansi

// 256-color palette for sources (modules ase-* and plugins ase-pl-*)
// Hash-based selection gives each unique source a consistent color
constexpr int SOURCE_COLORS[] = {
    208,  // Orange
    39,   // Deep sky blue
    34,   // Forest green
    214,  // Gold
    141,  // Light purple
    167,  // Salmon
    71,   // Muted green
    179,  // Muted yellow
    110,  // Steel blue
    175,  // Pink
    149,  // Light olive
    74,   // Cyan
    216,  // Light orange
    139,  // Purple
    108,  // Sage
    180,  // Tan
};
constexpr size_t SOURCE_COLOR_COUNT = sizeof(SOURCE_COLORS) / sizeof(SOURCE_COLORS[0]);

// =============================================================================
// TERMINAL FUNCTIONS
// =============================================================================

/**
 * Write text to the terminal unchanged - no prefix, no newline, no level.
 *
 * THE ONE CHANNEL BY WHICH ase-ecs REACHES THE TERMINAL, and it is deliberately not
 * ase::log. Its two callers draw: boot_logger.cpp rewrites a printed "[..]" line into
 * "[OK]" with a carriage return and an erase sequence, and it CLEARS the ase::log sinks for
 * the duration of the boot block (boot_logger.cpp detaches console, file, HTTP-ring and
 * counting sinks and restores them afterwards) so no log
 * record can interleave with the table. A line sent through ase::log during that window
 * would land in the queue this file just installed, not on the screen - and would arrive
 * prefixed with level, timestamp and category into an aligned, box-drawn table.
 *
 * fwrite is the primitive because printf, fprintf, sprintf and the three std streams each
 * carry a rule and fwrite carries none - the same line the tree drew in
 * tools/ase-cli/src/main.cpp and in core/ase-convert/include/ase/convert/console.hpp.
 * Log RECORDS still belong to ase::log; this pair is for drawing.
 *
 * @param text What the caller wants on screen. An empty string writes nothing.
 */
void write_terminal(const std::string& text);

/**
 * Push whatever is buffered to the terminal now.
 *
 * Separate from write_terminal so a caller that emits several pieces of one line pays for
 * one flush rather than one per piece. A progress line that arrives after the step it
 * announces is worse than none.
 */
void flush_terminal();

/**
 * Draw one piece of a sequence table and flush it at once - or nothing at all.
 *
 * Every drawing step of the boot and the shutdown view was a pair of write_terminal and
 * flush_terminal. The pair stands here once, together with the one condition both views share:
 * the table belongs to whoever owns the terminal. A tier server owns it; an App embedded in a
 * host that owns the process (APP_LIFE_HOST, e.g. Godot in the Vivarium client) does not, and
 * its views draw nothing while every on_start and on_stop still runs in the same order.
 *
 * @param render false when the App is host-owned (BootLoggerConfig / ShutdownConfig
 *               render_terminal_table)
 * @param text   What the caller wants on screen. An empty string writes nothing.
 */
void draw_terminal(bool render, const std::string& text);

/**
 * Get terminal width in columns.
 * Returns 80 as default if not detectable.
 */
int get_terminal_width();

/**
 * Generate a line of box-drawing characters (━) matching terminal width.
 */
std::string terminal_line();

/**
 * Get short timestamp: MM:SS.mmm
 * Used in boot/shutdown logs.
 */
std::string short_timestamp();

/**
 * Get full timestamp: HH:MM:SS.mmm
 * Used in detailed logging.
 */
std::string full_timestamp();

/**
 * Get color code index for source name (hash-based).
 * Works for both modules (ase-*) and plugins (ase-pl-*).
 * Returns consistent color for same source name.
 * @param source_name e.g., "ase-kernel", "ase-network", "ase-pl-sky"
 * @return Index into SOURCE_COLORS array
 */
size_t source_color_index(const char* source_name);

/**
 * Get ANSI color escape sequence for source (module or plugin).
 * Caller must provide buffer of at least 16 bytes.
 * @param source_name e.g., "ase-kernel", "ase-pl-erosion"
 * @param buffer Output buffer for escape sequence
 * @param buffer_size Size of buffer
 */
void source_color(const char* source_name, char* buffer, size_t buffer_size);

}  // namespace ase::ecs::internal
