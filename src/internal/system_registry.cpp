#include <ase/ecs/internal/system_registry.hpp>

namespace ase::ecs::internal {

void SystemRegistry::add_system(Schedule schedule, std::unique_ptr<System> system,
                                 std::string name, std::string source, std::string version,
                                 std::vector<std::string> run_after, int priority) {
    SystemInfo info{
        .name = std::move(name),
        .source = std::move(source),
        .version = std::move(version),
        .schedule = schedule,
        .run_after = std::move(run_after),
        .priority = priority
    };
    system_infos_.push_back(std::move(info));
    schedule_systems_[schedule].push_back(std::move(system));
}

std::vector<std::unique_ptr<System>>& SystemRegistry::systems_for(Schedule schedule) {
    return schedule_systems_[schedule];
}

const std::vector<std::unique_ptr<System>>& SystemRegistry::systems_for(Schedule schedule) const {
    static const std::vector<std::unique_ptr<System>> empty;
    auto it = schedule_systems_.find(schedule);
    return (it != schedule_systems_.end()) ? it->second : empty;
}

size_t SystemRegistry::total_count() const {
    size_t count = 0;
    for (const auto& [schedule, systems] : schedule_systems_) {
        count += systems.size();
    }
    return count;
}

const SystemInfo* SystemRegistry::find_info(const std::string& name) const {
    for (const auto& info : system_infos_) {
        if (info.name == name) {
            return &info;
        }
    }
    return nullptr;
}

}  // namespace ase::ecs::internal
