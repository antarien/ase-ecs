#pragma once

/**
 * ASE ECS Internal - System Registry
 *
 * @file        system_registry.hpp
 * @brief       System storage and metadata management
 * @description Stores systems organized by schedule with their metadata.
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @created     2026-02-01
 */

#include <ase/ecs/schedule.hpp>
#include <ase/ecs/system.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ase::ecs::internal {

// =============================================================================
// SystemInfo - Metadata for each registered system
// =============================================================================

struct SystemInfo {
    std::string name;
    std::string source;   // Module/Plugin name (e.g., "ase-network", "ase-pl-sky")
    std::string version;  // Module/Plugin version (e.g., "00.01.07.00057")
    Schedule schedule = Schedule::Integration;
    std::vector<std::string> run_after;
    int priority = 0;
};

// =============================================================================
// SystemRegistry - System storage and metadata
// =============================================================================

class SystemRegistry {
public:
    using SystemMap = std::unordered_map<Schedule, std::vector<std::unique_ptr<System>>>;

    SystemRegistry() = default;
    ~SystemRegistry() = default;

    // Non-copyable
    SystemRegistry(const SystemRegistry&) = delete;
    SystemRegistry& operator=(const SystemRegistry&) = delete;

    /**
     * Register a system with its metadata.
     */
    void add_system(Schedule schedule, std::unique_ptr<System> system,
                    std::string name, std::string source, std::string version,
                    std::vector<std::string> run_after, int priority);

    /**
     * Get all systems for a schedule.
     */
    std::vector<std::unique_ptr<System>>& systems_for(Schedule schedule);
    const std::vector<std::unique_ptr<System>>& systems_for(Schedule schedule) const;

    /**
     * Get all registered systems (mutable access for sorting).
     */
    SystemMap& all_systems() { return schedule_systems_; }
    const SystemMap& all_systems() const { return schedule_systems_; }

    /**
     * Get all system metadata.
     */
    std::vector<SystemInfo>& infos() { return system_infos_; }
    const std::vector<SystemInfo>& infos() const { return system_infos_; }

    /**
     * Get total number of systems.
     */
    size_t total_count() const;

    /**
     * Find SystemInfo by name (returns nullptr if not found).
     */
    const SystemInfo* find_info(const std::string& name) const;

private:
    SystemMap schedule_systems_;
    std::vector<SystemInfo> system_infos_;
};

}  // namespace ase::ecs::internal
