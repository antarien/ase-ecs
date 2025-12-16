#pragma once

/**
 * ASE ECS System Registry
 *
 * Auto-registration and dependency resolution for ECS Systems.
 * Systems register themselves via AUTO_REGISTER_SYSTEM macro.
 *
 * Usage:
 *   // In system .cpp file:
 *   AUTO_REGISTER_SYSTEM(MySystem, SystemPhase::Core, {"LogSystem"})
 *
 *   // In main:
 *   ase::ecs::World world;
 *   SystemRegistry::create_all_systems(world);  // Auto-creates all registered systems
 *   world.start();
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace ase::ecs {

class System;
class World;

// ============================================================================
// System Phase (execution order)
// ============================================================================

/**
 * Bootstrap phases for system initialization order
 * Lower phase = starts first, stops last
 */
enum class SystemPhase : int {
    Foundation = 0,    // Logging, Config, Memory
    Core = 10,         // ECS infrastructure
    Services = 20,     // Service registration
    Terrain = 30,      // Terrain systems
    Replication = 40,  // Sync, Authority, Broadcast
    Input = 45,        // Input processing (camera rotation, etc.) - before physics/agents
    Physics = 50,      // Physics simulation
    Agents = 60,       // AI, Weather, Erosion agents
    Network = 70,      // Network I/O
    Render = 80,       // Render data preparation
    Plugin = 90        // Plugin systems
};

inline const char* phase_name(SystemPhase phase) {
    switch (phase) {
        case SystemPhase::Foundation: return "Foundation";
        case SystemPhase::Core: return "Core";
        case SystemPhase::Services: return "Services";
        case SystemPhase::Terrain: return "Terrain";
        case SystemPhase::Replication: return "Replication";
        case SystemPhase::Input: return "Input";
        case SystemPhase::Physics: return "Physics";
        case SystemPhase::Agents: return "Agents";
        case SystemPhase::Network: return "Network";
        case SystemPhase::Render: return "Render";
        case SystemPhase::Plugin: return "Plugin";
        default: return "Unknown";
    }
}

// ============================================================================
// System Info (metadata for registration)
// ============================================================================

struct SystemInfo {
    std::string name;
    SystemPhase phase;
    std::vector<std::string> dependencies;
    std::function<std::unique_ptr<System>()> factory;
};

// ============================================================================
// System Registry (singleton)
// ============================================================================

/**
 * Global registry for auto-registered systems
 *
 * Systems register themselves at static initialization time.
 * World can then create all registered systems in dependency order.
 */
class SystemRegistry {
public:
    /**
     * Register a system (called by AUTO_REGISTER_SYSTEM macro)
     */
    static void register_system(
        const std::string& name,
        SystemPhase phase,
        std::vector<std::string> dependencies,
        std::function<std::unique_ptr<System>()> factory
    ) {
        auto& registry = instance();
        registry.systems_[name] = SystemInfo{
            .name = name,
            .phase = phase,
            .dependencies = std::move(dependencies),
            .factory = std::move(factory)
        };
    }

    /**
     * Get all registered system names
     */
    static std::vector<std::string> list_systems() {
        std::vector<std::string> names;
        auto& registry = instance();
        for (const auto& [name, info] : registry.systems_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * Get system info by name
     */
    static const SystemInfo* get_info(const std::string& name) {
        auto& registry = instance();
        auto it = registry.systems_.find(name);
        return it != registry.systems_.end() ? &it->second : nullptr;
    }

    /**
     * Create all registered systems in a World (respects phase order)
     */
    static void create_all_systems(World& world);

    /**
     * Get systems sorted by phase and dependencies
     */
    static std::vector<const SystemInfo*> get_sorted_systems();

private:
    static SystemRegistry& instance() {
        static SystemRegistry reg;
        return reg;
    }

    std::unordered_map<std::string, SystemInfo> systems_;
};

// ============================================================================
// AUTO_REGISTER_SYSTEM Macro
// ============================================================================

/**
 * Macro to auto-register an ECS System
 *
 * Usage:
 *   AUTO_REGISTER_SYSTEM(ChunkLookupSystem, SystemPhase::Terrain, {})
 *   AUTO_REGISTER_SYSTEM(MutationSystem, SystemPhase::Terrain, {"ChunkLookupSystem"})
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
