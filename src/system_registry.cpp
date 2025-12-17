#include <ase/ecs/system_registry.hpp>
#include <ase/ecs/ecs.hpp>

#include <algorithm>
#include <iostream>
#include <iomanip>

namespace ase::ecs {

std::vector<const SystemDescriptor*> SystemRegistry::get_sorted_systems() {
    // Get all active schedules and collect their systems
    std::vector<const SystemDescriptor*> all_systems;

    auto schedules = ScheduleRegistry::get_active_schedules();
    for (auto schedule : schedules) {
        auto systems = ScheduleRegistry::get_systems(schedule);
        for (const auto* desc : systems) {
            all_systems.push_back(desc);
        }
    }

    return all_systems;
}

void SystemRegistry::create_all_systems(World& world) {
    // Build the dependency graph
    ScheduleRegistry::build_graph();

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

    // Schedule colors
    auto schedule_color = [&](Schedule schedule) -> const char* {
        switch (schedule) {
            case Schedule::Startup:       return BLUE;
            case Schedule::PreUpdate:     return CYAN;
            case Schedule::Update:        return GREEN;
            case Schedule::PostUpdate:    return YELLOW;
            case Schedule::FixedUpdate:   return GREEN;
            case Schedule::Replication:   return MAGENTA;
            case Schedule::Persistence:   return RED;
            default:                      return WHITE;
        }
    };

    std::cout << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n";
    std::cout << "  ASE Schedule Bootstrap " << DIM << "(" << sorted.size() << " systems)" << RESET << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n";

    // Group by schedule
    Schedule current_schedule = static_cast<Schedule>(999);
    int system_num = 0;

    for (const auto* desc : sorted) {
        ++system_num;

        // New schedule header
        if (desc->schedule != current_schedule) {
            current_schedule = desc->schedule;
            std::cout << "\n";
            std::cout << "  " << schedule_color(current_schedule) << "┌─ "
                      << schedule_name(current_schedule) << RESET << "\n";
        }

        // System entry
        std::cout << "  " << schedule_color(current_schedule) << "│" << RESET << "  ";
        std::cout << WHITE << desc->name << RESET;

        // Dependencies
        if (!desc->after.empty()) {
            std::cout << DIM << " → ";
            for (size_t i = 0; i < desc->after.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << desc->after[i];
            }
            std::cout << RESET;
        }
        std::cout << "\n";

        // Create and add system to world
        if (desc->factory) {
            auto system = desc->factory();
            if (system) {
                system->set_phase(desc->priority);
                world.add_system_ptr(std::move(system));
            }
        }
    }

    std::cout << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n\n";
}

}  // namespace ase::ecs
