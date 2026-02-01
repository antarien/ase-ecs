#pragma once

/**
 * ASE ECS Internal - Terminal Utilities
 *
 * @file        terminal_utils.hpp
 * @brief       ANSI colors, timestamps, terminal width detection
 * @description Shared utilities for boot/shutdown visualization.
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
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
