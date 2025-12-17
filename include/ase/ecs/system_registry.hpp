#pragma once

/**
 * ASE ECS System Registry (Adapter Layer)
 *
 * Provides AUTO_REGISTER_SYSTEM macro that converts to the new Schedule system.
 * New code should use REGISTER_SYSTEM from schedule_registry.hpp instead.
 *
 * Usage:
 *   // Legacy (still works, converts internally):
 *   AUTO_REGISTER_SYSTEM(MySystem, SystemPhase::Core, {"LogSystem"})
 *
 *   // New (preferred):
 *   REGISTER_SYSTEM(MySystem)
 *       .in_schedule(Schedule::Startup)
 *       .run_after("LogSystem");
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "schedule.hpp"
#include "schedule_registry.hpp"

namespace ase::ecs {

class System;
class World;

// ============================================================================
// SystemPhase (Mapping Layer for AUTO_REGISTER_SYSTEM)
// ============================================================================

/**
 * Legacy phase enum - maps to named Schedules internally.
 * Only exists for AUTO_REGISTER_SYSTEM compatibility.
 * New code should use Schedule directly via REGISTER_SYSTEM.
 */
enum class SystemPhase : int {
    Foundation  = 10,   // → Startup
    Core        = 20,   // → Startup
    Services    = 30,   // → Startup
    Terrain     = 50,   // → FixedUpdate
    Replication = 60,   // → Replication
    Input       = 70,   // → PreUpdate
    Physics     = 80,   // → FixedUpdate
    Agents      = 90,   // → FixedUpdate
    Network     = 100,  // → Replication
    Render      = 110,  // → PostUpdate
    Plugin      = 150,  // → Update
};

/**
 * Convert SystemPhase to named Schedule
 */
constexpr Schedule phase_to_schedule(SystemPhase phase) {
    switch (phase) {
        case SystemPhase::Foundation:  return Schedule::Startup;
        case SystemPhase::Core:        return Schedule::Startup;
        case SystemPhase::Services:    return Schedule::Startup;
        case SystemPhase::Terrain:     return Schedule::FixedUpdate;
        case SystemPhase::Replication: return Schedule::Replication;
        case SystemPhase::Input:       return Schedule::PreUpdate;
        case SystemPhase::Physics:     return Schedule::FixedUpdate;
        case SystemPhase::Agents:      return Schedule::FixedUpdate;
        case SystemPhase::Network:     return Schedule::Replication;
        case SystemPhase::Render:      return Schedule::PostUpdate;
        case SystemPhase::Plugin:      return Schedule::Update;
        default:                       return Schedule::Update;
    }
}

/**
 * Get phase name for logging
 */
inline const char* phase_name(SystemPhase phase) {
    switch (phase) {
        case SystemPhase::Foundation:  return "Foundation";
        case SystemPhase::Core:        return "Core";
        case SystemPhase::Services:    return "Services";
        case SystemPhase::Terrain:     return "Terrain";
        case SystemPhase::Replication: return "Replication";
        case SystemPhase::Input:       return "Input";
        case SystemPhase::Physics:     return "Physics";
        case SystemPhase::Agents:      return "Agents";
        case SystemPhase::Network:     return "Network";
        case SystemPhase::Render:      return "Render";
        case SystemPhase::Plugin:      return "Plugin";
        default:                       return "Unknown";
    }
}

// ============================================================================
// SystemRegistry (Facade over ScheduleRegistry)
// ============================================================================

/**
 * Facade that wraps ScheduleRegistry for legacy code.
 * New code should use ScheduleRegistry directly.
 */
class SystemRegistry {
public:
    /**
     * Register a system (converts to ScheduleRegistry internally)
     */
    static void register_system(
        const std::string& name,
        SystemPhase phase,
        std::vector<std::string> dependencies,
        std::function<std::unique_ptr<System>()> factory
    ) {
        SystemDescriptor desc;
        desc.name = name;
        desc.schedule = phase_to_schedule(phase);
        desc.priority = static_cast<int>(phase);  // Use phase value for ordering within schedule
        desc.factory = std::move(factory);

        // Convert dependencies to run_after constraints
        for (auto& dep : dependencies) {
            desc.after.push_back(std::move(dep));
        }

        ScheduleRegistry::register_system(std::move(desc));
    }

    /**
     * Get all registered system names
     */
    static std::vector<std::string> list_systems() {
        return ScheduleRegistry::get_all_system_names();
    }

    /**
     * Create all registered systems in a World
     * Delegates to World::run_startup() for schedule-based execution
     */
    static void create_all_systems(World& world);

    /**
     * Get sorted systems (for logging)
     */
    static std::vector<const SystemDescriptor*> get_sorted_systems();
};

// ============================================================================
// AUTO_REGISTER_SYSTEM Macro
// ============================================================================

/**
 * Macro to auto-register an ECS System (converts to ScheduleRegistry)
 *
 * Usage:
 *   AUTO_REGISTER_SYSTEM(ChunkLookupSystem, SystemPhase::Terrain, {})
 *   AUTO_REGISTER_SYSTEM(MutationSystem, SystemPhase::Terrain, {"ChunkLookupSystem"})
 *
 * Prefer REGISTER_SYSTEM for new code:
 *   REGISTER_SYSTEM(MySystem)
 *       .in_schedule(Schedule::FixedUpdate)
 *       .run_after("ChunkLookupSystem");
 */
#define AUTO_REGISTER_SYSTEM(ClassName, Phase, Dependencies) \
    namespace { \
        struct ClassName##_AutoRegister { \
            ClassName##_AutoRegister() { \
                ::ase::ecs::SystemRegistry::register_system( \
                    #ClassName, \
                    Phase, \
                    Dependencies, \
                    []() -> std::unique_ptr<::ase::ecs::System> { \
                        return std::make_unique<ClassName>(); \
                    } \
                ); \
            } \
        }; \
        static ClassName##_AutoRegister ClassName##_auto_register_instance; \
    }

}  // namespace ase::ecs
