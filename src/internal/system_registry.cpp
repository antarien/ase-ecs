#include <ase/ecs/internal/system_registry.hpp>

namespace ase::ecs::internal {

void SystemRegistry::add_system(Schedule schedule, std::unique_ptr<System> system,
                                 std::string name, std::string source, std::string version,
                                 std::vector<std::string> run_after, int priority) {
    auto* sys_ptr = system.get();

    SystemInfo info{
        .name = name,
        .source = source,
        .version = std::move(version),
        .schedule = schedule,
        .run_after = std::move(run_after),
        .priority = priority
    };

    // Late-System-Registration: if boot is in progress, ONLY queue — do NOT
    // modify main lists (iterator invalidation in print_boot_sequence → SEGV).
    // boot_pending_systems() will add them to the main lists after boot completes.
    if (boot_started_) {
        pending_.push_back({sys_ptr, std::move(name), schedule,
                            std::move(info), std::move(system)});
        return;
    }

    // Normal registration (before boot)
    name_to_system_[name] = sys_ptr;
    size_t info_index = system_infos_.size();
    name_to_index_[name] = info_index;
    ++source_totals_[source];
    ++total_count_;

    system_infos_.push_back(std::move(info));
    schedule_systems_[schedule].push_back(std::move(system));
    schedule_info_indices_[schedule].push_back(info_index);
    ++schedule_source_counts_[schedule][source];
}

void SystemRegistry::add_deferred(Schedule schedule, std::unique_ptr<System> system,
                                   const SystemInfo& info) {
    auto* sys_ptr = system.get();
    name_to_system_[info.name] = sys_ptr;
    size_t info_index = system_infos_.size();
    name_to_index_[info.name] = info_index;
    ++source_totals_[info.source];
    ++total_count_;

    system_infos_.push_back(info);
    schedule_systems_[schedule].push_back(std::move(system));
    schedule_info_indices_[schedule].push_back(info_index);
    ++schedule_source_counts_[schedule][info.source];
}

std::vector<std::unique_ptr<System>>& SystemRegistry::systems_for(Schedule schedule) {
    return schedule_systems_[schedule];
}

const std::vector<std::unique_ptr<System>>& SystemRegistry::systems_for(Schedule schedule) const {
    static const std::vector<std::unique_ptr<System>> empty;
    auto it = schedule_systems_.find(schedule);
    return (it != schedule_systems_.end()) ? it->second : empty;
}

const SystemInfo* SystemRegistry::find_info(const std::string& name) const {
    auto it = name_to_index_.find(name);
    if (it == name_to_index_.end()) { return nullptr; }
    return &system_infos_[it->second];
}

System* SystemRegistry::find_system(const std::string& name) const {
    auto it = name_to_system_.find(name);
    if (it == name_to_system_.end()) { return nullptr; }
    return it->second;
}

size_t SystemRegistry::source_total(const std::string& source) const {
    auto it = source_totals_.find(source);
    if (it == source_totals_.end()) { return 0; }
    return it->second;
}

size_t SystemRegistry::schedule_source_count(Schedule schedule, const std::string& source) const {
    auto sched_it = schedule_source_counts_.find(schedule);
    if (sched_it == schedule_source_counts_.end()) { return 0; }
    auto src_it = sched_it->second.find(source);
    if (src_it == sched_it->second.end()) { return 0; }
    return src_it->second;
}

std::vector<PendingEntry> SystemRegistry::drain_pending() {
    return std::move(pending_);
}

uint32_t SystemRegistry::remove_systems_by_source(Registry& registry, const std::string& source) {
    uint32_t removed = 0;

    /**
     * Collect names of systems to remove (from system_infos_).
     */
    std::vector<std::string> names_to_remove;
    for (auto& info : system_infos_) {
        if (info.source == source) {
            names_to_remove.push_back(info.name);
        }
    }

    /**
     * Call on_stop() on each system before removal.
     */
    for (auto& sys_name : names_to_remove) {
        auto* sys = find_system(sys_name);
        if (sys) {
            sys->on_stop(registry);
        }
    }

    /**
     * Remove from schedule_systems_ (owns unique_ptr, destroys System objects).
     */
    for (auto& [schedule, systems] : schedule_systems_) {
        systems.erase(
            std::remove_if(systems.begin(), systems.end(),
                [&](const std::unique_ptr<System>& sys) {
                    auto* info = find_info(sys->name());
                    return info && info->source == source;
                }),
            systems.end());
    }

    /**
     * Remove from lookup maps.
     */
    for (auto& sys_name : names_to_remove) {
        name_to_system_.erase(sys_name);
        name_to_index_.erase(sys_name);
        ++removed;
    }

    /**
     * Rebuild schedule_info_indices_ and schedule_source_counts_ from system_infos_.
     * Remove entries for this source from system_infos_.
     */
    system_infos_.erase(
        std::remove_if(system_infos_.begin(), system_infos_.end(),
            [&](const SystemInfo& info) { return info.source == source; }),
        system_infos_.end());

    /**
     * Rebuild all index maps from scratch (system_infos_ changed).
     */
    name_to_index_.clear();
    schedule_info_indices_.clear();
    schedule_source_counts_.clear();
    source_totals_.clear();
    total_count_ = 0;

    for (size_t idx = 0; idx < system_infos_.size(); ++idx) {
        auto& info = system_infos_[idx];
        name_to_index_[info.name] = idx;
        schedule_info_indices_[info.schedule].push_back(idx);
        ++schedule_source_counts_[info.schedule][info.source];
        ++source_totals_[info.source];
        ++total_count_;
    }

    /**
     * Rebuild name_to_system_ from schedule_systems_.
     */
    name_to_system_.clear();
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto& sys : systems) {
            name_to_system_[sys->name()] = sys.get();
        }
    }

    return removed;
}

}  // namespace ase::ecs::internal
