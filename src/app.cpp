#include <ase/ecs/app.hpp>
#include <algorithm>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <queue>

namespace ase::ecs {

App::App() = default;

void App::finalize_system(Schedule schedule, std::unique_ptr<System> system,
                          std::vector<std::string> after, int priority) {
    SystemInfo info{
        .name = system->name(),
        .schedule = schedule,
        .run_after = std::move(after),
        .priority = priority
    };
    system_infos_.push_back(std::move(info));
    schedule_systems_[schedule].push_back(std::move(system));
}

void App::print_boot_log() {
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
            case Schedule::First:         return CYAN;
            case Schedule::PreUpdate:     return CYAN;
            case Schedule::FixedFirst:    return GREEN;
            case Schedule::FixedPreUpdate: return GREEN;
            case Schedule::FixedUpdate:   return GREEN;
            case Schedule::FixedPostUpdate: return GREEN;
            case Schedule::FixedLast:     return GREEN;
            case Schedule::Update:        return GREEN;
            case Schedule::PostUpdate:    return YELLOW;
            case Schedule::Replication:   return MAGENTA;
            case Schedule::Persistence:   return RED;
            case Schedule::Last:          return YELLOW;
            case Schedule::Shutdown:      return BLUE;
            default:                      return WHITE;
        }
    };

    // Count total systems
    size_t total_systems = system_infos_.size();

    std::cout << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n";
    std::cout << "  ASE Schedule Bootstrap " << DIM << "(" << total_systems << " systems)" << RESET << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n";

    // Define schedule order
    static const std::vector<Schedule> schedule_order = {
        Schedule::Startup,
        Schedule::First,
        Schedule::PreUpdate,
        Schedule::FixedFirst,
        Schedule::FixedPreUpdate,
        Schedule::FixedUpdate,
        Schedule::FixedPostUpdate,
        Schedule::FixedLast,
        Schedule::Update,
        Schedule::PostUpdate,
        Schedule::Replication,
        Schedule::Persistence,
        Schedule::Last,
        Schedule::Shutdown
    };

    // Group system infos by schedule
    std::unordered_map<Schedule, std::vector<const SystemInfo*>> grouped;
    for (const auto& info : system_infos_) {
        grouped[info.schedule].push_back(&info);
    }

    // Print by schedule order
    for (auto schedule : schedule_order) {
        auto it = grouped.find(schedule);
        if (it == grouped.end() || it->second.empty()) {
            continue;
        }

        std::cout << "\n";
        std::cout << "  " << schedule_color(schedule) << "┌─ "
                  << schedule_name(schedule) << RESET << "\n";

        for (const auto* info : it->second) {
            std::cout << "  " << schedule_color(schedule) << "│" << RESET << "  ";
            std::cout << WHITE << info->name << RESET;

            // Dependencies
            if (!info->run_after.empty()) {
                std::cout << DIM << " → ";
                for (size_t i = 0; i < info->run_after.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << info->run_after[i];
                }
                std::cout << RESET;
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n";
    std::cout << DIM << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << RESET << "\n\n";
}

void App::sort_systems_by_dependencies() {
    // Sort systems within each schedule based on dependencies (topological sort)
    for (auto& [schedule, systems] : schedule_systems_) {
        if (systems.size() <= 1) continue;

        // Build name -> index mapping
        std::unordered_map<std::string, size_t> name_to_idx;
        for (size_t i = 0; i < systems.size(); ++i) {
            name_to_idx[systems[i]->name()] = i;
        }

        // Find matching SystemInfo for each system
        std::unordered_map<std::string, const SystemInfo*> info_map;
        for (const auto& info : system_infos_) {
            if (info.schedule == schedule) {
                info_map[info.name] = &info;
            }
        }

        // Build adjacency list and in-degree
        std::vector<std::vector<size_t>> adj(systems.size());
        std::vector<int> in_degree(systems.size(), 0);

        for (size_t i = 0; i < systems.size(); ++i) {
            const auto* info = info_map[systems[i]->name()];
            if (!info) continue;

            for (const auto& dep : info->run_after) {
                auto it = name_to_idx.find(dep);
                if (it != name_to_idx.end()) {
                    // dep must run before i, so edge from dep to i
                    adj[it->second].push_back(i);
                    in_degree[i]++;
                }
            }
        }

        // Kahn's algorithm for topological sort
        std::queue<size_t> queue;
        for (size_t i = 0; i < systems.size(); ++i) {
            if (in_degree[i] == 0) {
                queue.push(i);
            }
        }

        std::vector<std::unique_ptr<System>> sorted;
        sorted.reserve(systems.size());

        while (!queue.empty()) {
            size_t u = queue.front();
            queue.pop();
            sorted.push_back(std::move(systems[u]));

            for (size_t v : adj[u]) {
                if (--in_degree[v] == 0) {
                    queue.push(v);
                }
            }
        }

        // If we got all systems, use sorted order; otherwise keep original
        if (sorted.size() == systems.size()) {
            systems = std::move(sorted);
        }
    }
}

void App::startup() {
    // Sort systems by dependencies
    sort_systems_by_dependencies();

    // Print boot log
    print_boot_log();

    // Call on_start() for ALL systems (initialization)
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto& system : systems) {
            system->on_start(world_.registry());
        }
    }

    // Run Startup schedule
    run_schedule(Schedule::Startup, 0.0f);

    running_.store(true);
    last_frame_time_ = Clock::now();
}

void App::shutdown() {
    // Run Shutdown schedule
    run_schedule(Schedule::Shutdown, 0.0f);

    // Call on_stop() for ALL systems (cleanup) in reverse order
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
            (*it)->on_stop(world_.registry());
        }
    }

    running_.store(false);
}

void App::tick(float dt) {
    if (dt > max_frame_time_) {
        dt = max_frame_time_;
    }

    // First
    run_schedule(Schedule::First, dt);

    // PreUpdate
    run_schedule(Schedule::PreUpdate, dt);

    // FixedUpdate (30Hz)
    fixed_accumulator_ += dt;
    while (fixed_accumulator_ >= fixed_dt_) {
        run_schedule(Schedule::FixedFirst, fixed_dt_);
        run_schedule(Schedule::FixedPreUpdate, fixed_dt_);
        run_schedule(Schedule::FixedUpdate, fixed_dt_);
        run_schedule(Schedule::FixedPostUpdate, fixed_dt_);
        run_schedule(Schedule::FixedLast, fixed_dt_);
        fixed_accumulator_ -= fixed_dt_;
    }

    // Update
    run_schedule(Schedule::Update, dt);

    // PostUpdate
    run_schedule(Schedule::PostUpdate, dt);

    // Replication (20Hz)
    replication_accumulator_ += dt;
    if (replication_accumulator_ >= replication_dt_) {
        run_schedule(Schedule::Replication, replication_dt_);
        replication_accumulator_ -= replication_dt_;
    }

    // Persistence (1Hz)
    persistence_accumulator_ += dt;
    if (persistence_accumulator_ >= persistence_dt_) {
        run_schedule(Schedule::Persistence, persistence_dt_);
        persistence_accumulator_ -= persistence_dt_;
    }

    // Last
    run_schedule(Schedule::Last, dt);
}

void App::run() {
    startup();

    while (running_.load()) {
        auto now = Clock::now();
        float frame_dt = Duration(now - last_frame_time_).count();
        last_frame_time_ = now;

        tick(frame_dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    shutdown();
}

void App::run_schedule(Schedule schedule, float dt) {
    auto it = schedule_systems_.find(schedule);
    if (it == schedule_systems_.end()) {
        return;
    }

    for (auto& system : it->second) {
        if (system && system->enabled()) {
            system->tick(world_.registry(), dt);
        }
    }
}

}  // namespace ase::ecs
