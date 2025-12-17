#include <ase/ecs/schedule_registry.hpp>

#include <algorithm>
#include <queue>
#include <unordered_set>
#include <stdexcept>

namespace ase::ecs {

ScheduleRegistry& ScheduleRegistry::instance() {
    static ScheduleRegistry registry;
    return registry;
}

void ScheduleRegistry::register_system(SystemDescriptor desc) {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    const std::string name = desc.name;
    const Schedule schedule = desc.schedule;

    // Store system
    reg.systems_[name] = std::move(desc);

    // Add to schedule group
    reg.systems_by_schedule_[schedule].push_back(name);

    // Invalidate sorted cache
    reg.graph_built_ = false;
}

void ScheduleRegistry::register_set(SystemSet set) {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    reg.sets_[set.name] = std::move(set);
    reg.graph_built_ = false;
}

std::vector<const SystemDescriptor*> ScheduleRegistry::get_systems(Schedule schedule) {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    // Build graph if not already done
    if (!reg.graph_built_) {
        reg.build_graph();
    }

    auto it = reg.sorted_systems_.find(schedule);
    if (it != reg.sorted_systems_.end()) {
        return it->second;
    }
    return {};
}

std::vector<Schedule> ScheduleRegistry::get_active_schedules() {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    std::vector<Schedule> schedules;
    for (const auto& [schedule, _] : reg.systems_by_schedule_) {
        schedules.push_back(schedule);
    }

    // Sort by schedule value
    std::sort(schedules.begin(), schedules.end(),
        [](Schedule a, Schedule b) {
            return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
        });

    return schedules;
}

const SystemDescriptor* ScheduleRegistry::get_system(const std::string& name) {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    auto it = reg.systems_.find(name);
    if (it != reg.systems_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ScheduleRegistry::build_graph() {
    auto& reg = instance();
    // Note: Caller must hold lock

    reg.sorted_systems_.clear();

    // Build sorted list for each schedule
    for (const auto& [schedule, system_names] : reg.systems_by_schedule_) {
        if (!reg.topological_sort(schedule)) {
            return false;  // Cycle detected
        }
    }

    reg.graph_built_ = true;
    return true;
}

bool ScheduleRegistry::is_built() {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);
    return reg.graph_built_;
}

void ScheduleRegistry::clear() {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    reg.systems_.clear();
    reg.systems_by_schedule_.clear();
    reg.sets_.clear();
    reg.sorted_systems_.clear();
    reg.graph_built_ = false;
}

std::vector<std::string> ScheduleRegistry::get_all_system_names() {
    auto& reg = instance();
    std::lock_guard<std::mutex> lock(reg.mutex_);

    std::vector<std::string> names;
    names.reserve(reg.systems_.size());
    for (const auto& [name, _] : reg.systems_) {
        names.push_back(name);
    }
    return names;
}

bool ScheduleRegistry::topological_sort(Schedule schedule) {
    // Kahn's algorithm for topological sorting

    auto it = systems_by_schedule_.find(schedule);
    if (it == systems_by_schedule_.end()) {
        return true;  // No systems for this schedule
    }

    const auto& system_names = it->second;

    // Build in-degree map and adjacency list for this schedule
    std::unordered_map<std::string, int> in_degree;
    std::unordered_map<std::string, std::vector<std::string>> adj;

    // Initialize
    for (const auto& name : system_names) {
        in_degree[name] = 0;
        adj[name] = {};
    }

    // Build edges from ordering constraints
    for (const auto& name : system_names) {
        const auto& desc = systems_[name];

        // "run_after" means: dependency -> this system
        for (const auto& dep : desc.after) {
            // Only consider dependencies within the same schedule
            if (in_degree.count(dep)) {
                adj[dep].push_back(name);
                in_degree[name]++;
            }
        }

        // "run_before" means: this system -> target
        for (const auto& target : desc.before) {
            if (in_degree.count(target)) {
                adj[name].push_back(target);
                in_degree[target]++;
            }
        }
    }

    // Find all nodes with in-degree 0
    // Use priority queue to sort by priority within same in-degree
    auto cmp = [this](const std::string& a, const std::string& b) {
        return systems_[a].priority > systems_[b].priority;  // min-heap
    };
    std::priority_queue<std::string, std::vector<std::string>, decltype(cmp)> queue(cmp);

    for (const auto& [name, degree] : in_degree) {
        if (degree == 0) {
            queue.push(name);
        }
    }

    // Process
    std::vector<const SystemDescriptor*> sorted;
    sorted.reserve(system_names.size());

    while (!queue.empty()) {
        const std::string current = queue.top();
        queue.pop();

        sorted.push_back(&systems_[current]);

        for (const auto& neighbor : adj[current]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // Check for cycle
    if (sorted.size() != system_names.size()) {
        // Cycle detected
        return false;
    }

    sorted_systems_[schedule] = std::move(sorted);
    return true;
}

bool ScheduleRegistry::has_cycle(Schedule schedule) const {
    // Simple DFS-based cycle detection
    auto it = systems_by_schedule_.find(schedule);
    if (it == systems_by_schedule_.end()) {
        return false;
    }

    const auto& system_names = it->second;

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> rec_stack;

    std::function<bool(const std::string&)> dfs = [&](const std::string& name) -> bool {
        visited.insert(name);
        rec_stack.insert(name);

        auto sys_it = systems_.find(name);
        if (sys_it == systems_.end()) return false;

        const auto& desc = sys_it->second;

        // Check "before" edges
        for (const auto& target : desc.before) {
            if (systems_by_schedule_.at(schedule).end() !=
                std::find(systems_by_schedule_.at(schedule).begin(),
                         systems_by_schedule_.at(schedule).end(), target)) {
                if (!visited.count(target)) {
                    if (dfs(target)) return true;
                } else if (rec_stack.count(target)) {
                    return true;
                }
            }
        }

        rec_stack.erase(name);
        return false;
    };

    for (const auto& name : system_names) {
        if (!visited.count(name)) {
            if (dfs(name)) return true;
        }
    }

    return false;
}

} // namespace ase::ecs
