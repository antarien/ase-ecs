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
// Registration Helper
// ============================================================================

/**
 * Helper class that enables fluent registration with proper timing.
 *
 * Created as a temporary by REGISTER_SYSTEM macro. When the temporary
 * is destroyed (at end of the full expression), it registers the descriptor.
 * This ensures registration happens during static initialization, AFTER
 * all fluent method calls have been applied to the descriptor.
 */
class RegistrationHelper {
    SystemDescriptor* desc_;

public:
    explicit RegistrationHelper(SystemDescriptor* desc) : desc_(desc) {}

    ~RegistrationHelper() {
        if (desc_) {
            ScheduleRegistry::register_system(std::move(*desc_));
        }
    }

    // Disable copy to prevent double-registration
    RegistrationHelper(const RegistrationHelper&) = delete;
    RegistrationHelper& operator=(const RegistrationHelper&) = delete;

    // Enable move (transfers ownership)
    RegistrationHelper(RegistrationHelper&& other) noexcept : desc_(other.desc_) {
        other.desc_ = nullptr;
    }
    RegistrationHelper& operator=(RegistrationHelper&& other) noexcept {
        desc_ = other.desc_;
        other.desc_ = nullptr;
        return *this;
    }

    // Fluent methods - forward to descriptor and return *this
    RegistrationHelper& in_schedule(Schedule s) {
        desc_->schedule = s;
        return *this;
    }

    RegistrationHelper& with_priority(int p) {
        desc_->priority = p;
        return *this;
    }

    RegistrationHelper& run_after(const std::string& system_name) {
        desc_->after.push_back(system_name);
        return *this;
    }

    RegistrationHelper& run_before(const std::string& system_name) {
        desc_->before.push_back(system_name);
        return *this;
    }

    RegistrationHelper& run_if(RunCondition condition) {
        desc_->run_conditions.push_back(std::move(condition));
        return *this;
    }

    RegistrationHelper& in_set(const std::string& set_name) {
        desc_->in_sets.push_back(set_name);
        return *this;
    }

    // Conversion to int allows use in static variable initialization
    operator int() const { return 0; }
};

// ============================================================================
// Registration Macros
// ============================================================================

// NOTE: AUTO_REGISTER_SYSTEM is defined in system_registry.hpp (legacy)
// Use REGISTER_SYSTEM for new code with fluent API

/**
 * Fluent macro for system registration
 *
 * Usage:
 *   REGISTER_SYSTEM(MySystem)
 *       .in_schedule(Schedule::FixedUpdate)
 *       .run_after("ChunkLookupSystem")
 *       .run_if(conditions::any_with_component<Dirty>());
 *
 * How it works:
 *   1. Creates a static SystemDescriptor initialized with name and factory
 *   2. Creates a temporary RegistrationHelper pointing to the descriptor
 *   3. Fluent method calls modify the descriptor
 *   4. When the temporary RegistrationHelper is destroyed (end of statement),
 *      it registers the descriptor with ScheduleRegistry
 *   5. The RegistrationHelper converts to int (0) to initialize the static bool
 */
#define REGISTER_SYSTEM(ClassName) \
    static ::ase::ecs::SystemDescriptor ClassName##_descriptor_ = []{ \
        ::ase::ecs::SystemDescriptor d; \
        d.name = #ClassName; \
        d.factory = []() -> std::unique_ptr<::ase::ecs::System> { \
            return std::make_unique<ClassName>(); \
        }; \
        return d; \
    }(); \
    [[maybe_unused]] static int ClassName##_registered_ = ::ase::ecs::RegistrationHelper(&ClassName##_descriptor_)

} // namespace ase::ecs
