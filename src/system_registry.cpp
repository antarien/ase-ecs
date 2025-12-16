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

    // ANSI colors
    constexpr const char* RESET = "\x1b[0m";
    constexpr const char* DIM = "\x1b[38;5;243m";
    constexpr const char* CYAN = "\x1b[36m";
    constexpr const char* GREEN = "\x1b[32m";
    constexpr const char* YELLOW = "\x1b[33m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* BLUE = "\x1b[34m";
    constexpr const char* RED = "\x1b[31m";
    constexpr const char* WHITE = "\x1b[37m";

    // Phase colors
    auto phase_color = [&](SystemPhase phase) -> const char* {
        switch (phase) {
            case SystemPhase::Foundation: return BLUE;
            case SystemPhase::Core:       return CYAN;
            case SystemPhase::Terrain:    return GREEN;
            case SystemPhase::Replication:return YELLOW;
            case SystemPhase::Agents:     return MAGENTA;
            case SystemPhase::Network:    return RED;
            case SystemPhase::Render:     return WHITE;
            case SystemPhase::Plugin:     return CYAN;
            default:                      return RESET;
        }
    };

    std::cout << DIM << "ASE " << RESET << "ECS Bootstrap (" << sorted.size() << " systems)\n";

    // Group by phase and print compact
    SystemPhase current_phase = static_cast<SystemPhase>(-1);
    bool first_in_phase = true;

    for (const auto* info : sorted) {
        if (info->phase != current_phase) {
            if (current_phase != static_cast<SystemPhase>(-1)) {
                std::cout << "\n";  // End previous phase line
            }
            current_phase = info->phase;
            first_in_phase = true;
            std::cout << phase_color(current_phase) << std::setw(11) << std::left
                      << phase_name(current_phase) << RESET << " ";
        }

        if (!first_in_phase) {
            std::cout << DIM << "· " << RESET;
        }
        first_in_phase = false;

        std::cout << info->name;
        if (!info->dependencies.empty()) {
            std::cout << DIM << "(" << info->dependencies[0];
            for (size_t i = 1; i < info->dependencies.size(); ++i) {
                std::cout << "," << info->dependencies[i];
            }
            std::cout << ")" << RESET;
        }

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
