#include <ase/ecs/system_registry.hpp>
#include <ase/ecs/ecs.hpp>

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>

namespace ase::ecs {

std::vector<const SystemInfo*> SystemRegistry::get_sorted_systems() {
    auto& registry = instance();
    std::vector<const SystemInfo*> sorted;

    for (const auto& [name, info] : registry.systems_) {
        sorted.push_back(&info);
    }

    // Sort by phase first, then by dependency order within phase
    std::sort(sorted.begin(), sorted.end(), [](const SystemInfo* a, const SystemInfo* b) {
        if (static_cast<int>(a->phase) != static_cast<int>(b->phase)) {
            return static_cast<int>(a->phase) < static_cast<int>(b->phase);
        }
        // Within same phase: if b depends on a, a comes first
        for (const auto& dep : b->dependencies) {
            if (dep == a->name) return true;
        }
        return a->name < b->name;  // Alphabetical fallback
    });

    return sorted;
}

void SystemRegistry::create_all_systems(World& world) {
    auto sorted = get_sorted_systems();

    std::cout << "\n";
    std::cout << "\x1b[38;5;243m" << "ASE - ANTARES SIMULATION ENGINE" << "\x1b[0m\n";
    std::cout << "\x1b[38;5;243m" << "ECS System Bootstrap" << "\x1b[0m\n";
    std::cout << "\n";
    std::cout << "[ASE] Registered systems: " << sorted.size() << "\n";
    std::cout << "\n";

    // Show dependency tree (phase_name is a free function in ase::ecs namespace)
    SystemPhase current_phase = static_cast<SystemPhase>(-1);
    for (const auto* info : sorted) {
        if (info->phase != current_phase) {
            current_phase = info->phase;
            std::cout << "[ASE] [Phase: " << phase_name(current_phase) << "]\n";
        }

        std::cout << "[ASE]   + " << info->name;
        if (!info->dependencies.empty()) {
            std::cout << " (deps: ";
            for (size_t i = 0; i < info->dependencies.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << info->dependencies[i];
            }
            std::cout << ")";
        }
        std::cout << "\n";

        // Create system via factory and set its phase
        auto system = info->factory();
        if (system) {
            system->set_phase(static_cast<int>(info->phase));
            world.add_system_ptr(std::move(system));
        }
    }

    std::cout << "\n";
}

}  // namespace ase::ecs
