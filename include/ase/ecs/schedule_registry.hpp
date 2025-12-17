#pragma once

/**
 * @file schedule_registry.hpp
 * @brief Global registry for scheduled systems
 *
 * Manages system registration and provides topologically sorted systems per schedule.
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "schedule.hpp"
#include "system_descriptor.hpp"

namespace ase::ecs {

/**
 * Global registry for scheduled systems.
 * Thread-safe singleton that manages system registration and sorting.
 */
class ScheduleRegistry {
public:
    /**
     * Register a system with its descriptor
     */
    static void register_system(SystemDescriptor desc);

    /**
     * Register a system set
     */
    static void register_set(SystemSet set);

    /**
     * Get all system descriptors for a schedule, topologically sorted
     */
    static std::vector<const SystemDescriptor*> get_systems(Schedule schedule);

    /**
     * Get all schedules that have registered systems
     */
    static std::vector<Schedule> get_active_schedules();

    /**
     * Get a system descriptor by name
     */
    static const SystemDescriptor* get_system(const std::string& name);

    /**
     * Build the execution graph (call once after all systems registered)
     * Returns false if cycles detected
     */
    static bool build_graph();

    /**
     * Check if graph has been built
     */
    static bool is_built();

    /**
     * Clear all registrations (for testing)
     */
    static void clear();

    /**
     * Get all registered system names (for debugging)
     */
    static std::vector<std::string> get_all_system_names();

private:
    static ScheduleRegistry& instance();

    ScheduleRegistry() = default;

    // Topological sort for a single schedule
    bool topological_sort(Schedule schedule);

    // Check for cycles in the dependency graph
    bool has_cycle(Schedule schedule) const;

    std::mutex mutex_;

    // All registered systems by name
    std::unordered_map<std::string, SystemDescriptor> systems_;

    // Systems grouped by schedule (unsorted)
    std::unordered_map<Schedule, std::vector<std::string>> systems_by_schedule_;

    // System sets
    std::unordered_map<std::string, SystemSet> sets_;

    // Topologically sorted systems per schedule (computed by build_graph)
    std::unordered_map<Schedule, std::vector<const SystemDescriptor*>> sorted_systems_;

    bool graph_built_ = false;
};

// ============================================================================
// Registration Macros
// ============================================================================

// NOTE: AUTO_REGISTER_SYSTEM is defined in system_registry.hpp (legacy)
// Use REGISTER_SYSTEM for new code with fluent API

/**
 * New fluent macro for system registration
 *
 * Usage:
 *   REGISTER_SYSTEM(MySystem)
 *       .in_schedule(Schedule::FixedUpdate)
 *       .run_after("ChunkLookupSystem")
 *       .run_if(conditions::any_with_component<Dirty>());
 */
#define REGISTER_SYSTEM(ClassName) \
    namespace { \
        struct ClassName##_FluentRegister { \
            ::ase::ecs::SystemDescriptor desc_; \
            ClassName##_FluentRegister() { \
                desc_.name = #ClassName; \
                desc_.factory = []() -> std::unique_ptr<::ase::ecs::System> { \
                    return std::make_unique<ClassName>(); \
                }; \
            } \
            ~ClassName##_FluentRegister() { \
                ::ase::ecs::ScheduleRegistry::register_system(std::move(desc_)); \
            } \
            ::ase::ecs::SystemDescriptor& operator()() { return desc_; } \
        }; \
        static auto ClassName##_fluent_register_ = ClassName##_FluentRegister()()

// Note: The REGISTER_SYSTEM macro returns the descriptor reference,
// allowing chained method calls. The destructor registers the system.

} // namespace ase::ecs
