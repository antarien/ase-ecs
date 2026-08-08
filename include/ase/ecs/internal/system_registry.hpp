#pragma once

/**
 * ASE ECS Internal - System Registry
 *
 * @file        system_registry.hpp
 * @brief       System storage and metadata management
 * @description Stores systems organized by schedule with their metadata.
 *              All lookups are O(1) via persistent hash maps populated at insert time.
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
#include <ase/containers/hash_map.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

// =============================================================================
// SystemInfo - Metadata for each registered system
// =============================================================================

struct SystemInfo {
    std::string name;
    std::string source;   // Module/Plugin name (e.g., "ase-network", "ase-pl-sky")
    std::string version;  // Module/Plugin version (e.g., "00.01.07.00057")
    Schedule schedule = Schedule::Integration;
    ase::containers::Vector<std::string> run_after;
    int priority = 0;
    // M-B module axis (PLAN_ASE_COMPUTE_MOD_AXIS.md T3): System→Module is known
    // HERE, at registration time - both fields are always resolved by add_system
    // (never observable as defaults) so the tick loop attributes O(1).
    uint32_t mod_hash = 0;  // FNV-1a32 (entt::hashed_string) of source
    uint32_t grp_id = 0;    // module-group id (ase::types::mod_grp_of; MOD_GRP_ID_NONE = unregistered)
};

// =============================================================================
// PendingEntry - System added after boot started (Late-System-Registration)
// =============================================================================

struct PendingEntry {
    System* system = nullptr;
    std::string name;
    Schedule schedule = Schedule::Integration;
    SystemInfo info;
    std::unique_ptr<System> owned_system;
};

// =============================================================================
// SystemRegistry - System storage and metadata (O(1) lookups)
// =============================================================================

class SystemRegistry {
public:
    using SystemMap = ase::containers::HashMap<Schedule, ase::containers::Vector<std::unique_ptr<System>>>;

    SystemRegistry() = default;
    ~SystemRegistry() = default;

    // Non-copyable
    SystemRegistry(const SystemRegistry&) = delete;
    SystemRegistry& operator=(const SystemRegistry&) = delete;

    /**
     * Register a system with its metadata.
     * Populates all persistent maps at insert time (O(1) amortized).
     * If boot has started, also queues the system as pending.
     */
    void add_system(Schedule schedule, std::unique_ptr<System> system,
                    std::string name, std::string source, std::string version,
                    ase::containers::Vector<std::string> run_after, int priority);

    /**
     * Add a deferred system to the main lists (called after boot completes).
     * Used by boot_pending_systems to safely insert late-registered systems.
     */
    void add_deferred(Schedule schedule, std::unique_ptr<System> system,
                      const SystemInfo& info);

    /**
     * Get all systems for a schedule.
     */
    ase::containers::Vector<std::unique_ptr<System>>& systems_for(Schedule schedule);
    const ase::containers::Vector<std::unique_ptr<System>>& systems_for(Schedule schedule) const;

    /**
     * Get all registered systems (mutable access for sorting).
     */
    SystemMap& all_systems() { return schedule_systems_; }
    const SystemMap& all_systems() const { return schedule_systems_; }

    /**
     * Apply a permutation to both schedule_systems_[schedule] AND
     * schedule_info_indices_[schedule] in lock-step, so the boot logger
     * (which iterates schedule_info_indices_) and the tick loop (which
     * iterates schedule_systems_) stay in sync after dependency sorting.
     *
     * permutation[i] = old index of the system that should land at position i.
     * Must have the same size as schedule_systems_[schedule].
     */
    void reorder_schedule(Schedule schedule, const ase::containers::Vector<size_t>& permutation);

    /**
     * Get all system metadata.
     */
    ase::containers::Vector<SystemInfo>& infos() { return system_infos_; }
    const ase::containers::Vector<SystemInfo>& infos() const { return system_infos_; }

    /**
     * Get system info indices grouped by schedule. O(1) lookup per schedule.
     * Indices into system_infos_ (stable across vector reallocation).
     */
    const ase::containers::HashMap<Schedule, ase::containers::Vector<size_t>>&
        infos_by_schedule() const { return schedule_info_indices_; }

    /**
     * Get total number of systems. O(1) cached.
     */
    size_t total_count() const { return total_count_; }

    /**
     * Find SystemInfo by name. O(1) hash lookup.
     */
    const SystemInfo* find_info(const std::string& name) const;

    /**
     * Find System pointer by name. O(1) hash lookup.
     * Used by boot_logger to avoid building temporary maps.
     */
    System* find_system(const std::string& name) const;

    /**
     * Get total systems count for a source (module/plugin). O(1) hash lookup.
     * Used by boot_logger for module-local counters.
     */
    size_t source_total(const std::string& source) const;

    /**
     * Get systems count for a source within a specific schedule. O(1) hash lookup.
     * Used by boot_logger for visual grouping (empty lines between modules).
     */
    size_t schedule_source_count(Schedule schedule, const std::string& source) const;

    // =========================================================================
    // Late-System-Registration (dlopen Module Support)
    // =========================================================================

    /**
     * Mark that boot has started. Systems added after this point are queued.
     */
    void mark_boot_started() { boot_started_ = true; }

    /**
     * Check if new systems were added since boot started.
     */
    bool has_pending() const { return !pending_.empty(); }

    /**
     * Drain the pending queue. Returns all pending entries and clears the queue.
     * O(1) move.
     */
    ase::containers::Vector<PendingEntry> drain_pending();

    // =========================================================================
    // Hot-Reload Support (Phase 9)
    // =========================================================================

    /**
     * Remove all systems registered by a specific source (module/plugin name).
     * Called before dlclose() during hot-reload. Removes from schedule_systems_,
     * system_infos_, and all lookup maps. Calls on_stop() on each removed system.
     *
     * @param registry  ECS Registry for calling on_stop()
     * @param source    Module/plugin name (e.g., "ase-pl-sky")
     * @return          Number of systems removed
     */
    uint32_t remove_systems_by_source(Registry& registry, const std::string& source);

private:
    SystemMap schedule_systems_;
    ase::containers::Vector<SystemInfo> system_infos_;

    // O(1) lookup maps (populated at insert time by add_system)
    ase::containers::HashMap<std::string, System*> name_to_system_;
    ase::containers::HashMap<std::string, size_t> name_to_index_;
    ase::containers::HashMap<std::string, size_t> source_totals_;
    ase::containers::HashMap<Schedule, ase::containers::Vector<size_t>> schedule_info_indices_;
    ase::containers::HashMap<Schedule, ase::containers::HashMap<std::string, size_t>> schedule_source_counts_;
    size_t total_count_ = 0;

    // Late-System-Registration
    bool boot_started_ = false;
    ase::containers::Vector<PendingEntry> pending_;
};

}  // namespace ase::ecs::internal
