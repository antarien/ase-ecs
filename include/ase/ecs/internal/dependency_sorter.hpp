#pragma once

/**
 * ASE ECS Internal - Dependency Sorter
 *
 * @file        dependency_sorter.hpp
 * @brief       Topological sort for system dependencies
 * @description Sorts systems within each schedule based on run_after dependencies
 *              using Kahn's algorithm. Returns error on cycle detection.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
 */

#include <ase/ecs/internal/system_registry.hpp>

#include <string>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

// =============================================================================
// CYCLE DETECTION RESULT
// =============================================================================

struct CycleError {
    Schedule schedule;
    ase::containers::Vector<std::string> cycle_participants;  // Systems involved in cycle
};

// =============================================================================
// DEPENDENCY SORTER
// =============================================================================

/**
 * Sort systems within each schedule by their dependencies.
 *
 * Uses Kahn's algorithm for topological sort.
 *
 * IMPORTANT: This function stores INDICES during sorting, and only
 * reorders the actual pointers AFTER confirming no cycle exists.
 * This prevents the null pointer bug where moving during iteration
 * would corrupt the vector on cycle detection.
 *
 * @param registry System registry to sort (modified in-place)
 * @return Empty vector on success, list of CycleErrors on failure
 */
ase::containers::Vector<CycleError> sort_systems_by_dependencies(SystemRegistry& registry);

}  // namespace ase::ecs::internal
