#pragma once

/**
 * @file system_descriptor.hpp
 * @brief System descriptor with fluent API for configuration
 *
 * Describes a system with its schedule, ordering constraints, and run conditions.
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "schedule.hpp"
#include "run_condition.hpp"

namespace ase::ecs {

// Forward declaration
class System;

/**
 * Describes a system with its schedule, ordering, and conditions.
 * Uses fluent API for configuration.
 *
 * Example:
 *   SystemDescriptor desc;
 *   desc.name = "MySystem";
 *   desc.in_schedule(Schedule::FixedUpdate)
 *       .run_after("ChunkLookupSystem")
 *       .run_before("ReplicationSystem")
 *       .run_if(conditions::any_with_component<Dirty>());
 */
struct SystemDescriptor {
    /// System name (used for ordering and logging)
    std::string name;

    /// Schedule this system belongs to
    Schedule schedule = Schedule::Update;

    /// Priority within the schedule (lower = earlier, default 0)
    int priority = 0;

    /// Systems this should run BEFORE
    std::vector<std::string> before;

    /// Systems this should run AFTER
    std::vector<std::string> after;

    /// Run conditions (all must be true for system to execute)
    std::vector<RunCondition> run_conditions;

    /// System sets this belongs to (for group ordering)
    std::vector<std::string> in_sets;

    /// Factory function to create system instance
    std::function<std::unique_ptr<System>()> factory;

    // === Fluent API ===

    /**
     * Set the schedule for this system
     */
    SystemDescriptor& in_schedule(Schedule s) {
        schedule = s;
        return *this;
    }

    /**
     * Set priority within schedule (lower = earlier)
     */
    SystemDescriptor& with_priority(int p) {
        priority = p;
        return *this;
    }

    /**
     * This system should run BEFORE another system
     */
    SystemDescriptor& run_before(const std::string& system_name) {
        before.push_back(system_name);
        return *this;
    }

    /**
     * This system should run AFTER another system
     */
    SystemDescriptor& run_after(const std::string& system_name) {
        after.push_back(system_name);
        return *this;
    }

    /**
     * Add a run condition (system runs only if condition is true)
     */
    SystemDescriptor& run_if(RunCondition condition) {
        run_conditions.push_back(std::move(condition));
        return *this;
    }

    /**
     * Add this system to a system set
     */
    SystemDescriptor& in_set(const std::string& set_name) {
        in_sets.push_back(set_name);
        return *this;
    }

    /**
     * Set the factory function
     */
    template<typename T>
    SystemDescriptor& with_factory() {
        factory = []() -> std::unique_ptr<System> {
            return std::make_unique<T>();
        };
        return *this;
    }
};

/**
 * A system set for grouping and ordering systems together.
 *
 * All systems in a set inherit the set's ordering constraints and conditions.
 */
struct SystemSet {
    /// Set name
    std::string name;

    /// Schedule for all systems in this set
    Schedule schedule = Schedule::Update;

    /// Sets/systems this set should run BEFORE
    std::vector<std::string> before;

    /// Sets/systems this set should run AFTER
    std::vector<std::string> after;

    /// Run conditions for the entire set
    std::vector<RunCondition> run_conditions;

    // === Fluent API ===

    SystemSet& in_schedule(Schedule s) {
        schedule = s;
        return *this;
    }

    SystemSet& run_before(const std::string& name) {
        before.push_back(name);
        return *this;
    }

    SystemSet& run_after(const std::string& name) {
        after.push_back(name);
        return *this;
    }

    SystemSet& run_if(RunCondition condition) {
        run_conditions.push_back(std::move(condition));
        return *this;
    }
};

} // namespace ase::ecs
