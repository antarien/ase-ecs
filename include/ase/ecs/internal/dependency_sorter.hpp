#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        dependency_sorter.hpp
 * @brief       Topological sort for system dependencies
 * @description Sorts systems within each schedule based on run_after dependencies
 *              using Kahn's algorithm. Returns error on cycle detection.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation/algorithm
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
