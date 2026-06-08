#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/log/log.hpp>

#include <queue>
#include <ase/containers/hash_map.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

ase::containers::Vector<CycleError> sort_systems_by_dependencies(SystemRegistry& registry) {
    ase::containers::Vector<CycleError> errors;

    for (auto& [schedule, systems] : registry.all_systems()) {
        if (systems.size() <= 1) continue;

        // Build name → index mapping
        ase::containers::HashMap<std::string, size_t> name_to_idx;
        for (size_t i = 0; i < systems.size(); ++i) {
            if (systems[i]) {
                name_to_idx[systems[i]->name()] = i;
            }
        }

        // Find matching SystemInfo for each system
        ase::containers::HashMap<std::string, const SystemInfo*> info_map;
        for (const auto& info : registry.infos()) {
            if (info.schedule == schedule) {
                info_map[info.name] = &info;
            }
        }

        // Build adjacency list and in-degree
        ase::containers::Vector<ase::containers::Vector<size_t>> adj(systems.size());
        ase::containers::Vector<int> in_degree(systems.size(), 0);

        for (size_t i = 0; i < systems.size(); ++i) {
            if (!systems[i]) continue;

            auto it = info_map.find(systems[i]->name());
            if (it == info_map.end()) continue;

            const auto* info = it->second;
            for (const auto& dep : info->run_after) {
                auto dep_it = name_to_idx.find(dep);
                if (dep_it != name_to_idx.end()) {
                    // dep must run before i, so edge from dep → i
                    adj[dep_it->second].push_back(i);
                    in_degree[i]++;
                }
            }
        }

        // Kahn's algorithm: store INDICES, not pointers!
        // This is the FIX: we don't move any pointers until we confirm no cycle.
        std::queue<size_t> queue;
        for (size_t i = 0; i < systems.size(); ++i) {
            if (in_degree[i] == 0) {
                queue.push(i);
            }
        }

        ase::containers::Vector<size_t> sorted_indices;
        sorted_indices.reserve(systems.size());

        while (!queue.empty()) {
            size_t u = queue.front();
            queue.pop();
            sorted_indices.push_back(u);

            for (size_t v : adj[u]) {
                if (--in_degree[v] == 0) {
                    queue.push(v);
                }
            }
        }

        // Check for cycle
        if (sorted_indices.size() != systems.size()) {
            // Cycle detected! Find participants (nodes with remaining in-degree)
            CycleError error;
            error.schedule = schedule;
            for (size_t i = 0; i < systems.size(); ++i) {
                if (in_degree[i] > 0 && systems[i]) {
                    error.cycle_participants.push_back(systems[i]->name());
                }
            }

            log::error("[DependencySorter] Cycle detected in schedule {}: {}",
                       schedule_name(schedule),
                       error.cycle_participants.empty() ? "unknown" : error.cycle_participants[0]);

            errors.push_back(std::move(error));
            // Keep original order: systems vector is UNCHANGED (no nulls!)
            continue;
        }

        // No cycle: apply the permutation to BOTH schedule_systems_ (tick loop)
        // AND schedule_info_indices_ (boot logger) atomically, so on_start order
        // matches tick order. Previously only schedule_systems_ was reordered,
        // which caused run_after to be silently ignored during boot.
        registry.reorder_schedule(schedule, sorted_indices);
    }

    return errors;
}

}  // namespace ase::ecs::internal
