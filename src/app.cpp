#include <ase/ecs/app.hpp>
#include <ase/log/log.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <queue>
#include <iomanip>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace ase::ecs {

namespace {

// Get current timestamp in format [YYYY-MM-DD HH:MM:SS.mmm]
std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Get terminal width (default 80 if not available)
int get_terminal_width() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    // Fallback to COLUMNS env var
    if (const char* cols = std::getenv("COLUMNS")) {
        int width = std::atoi(cols);
        if (width > 0) return width;
    }
    return 80;  // Default
}

// Generate a line of ━ characters matching terminal width
std::string terminal_line() {
    int width = get_terminal_width();
    std::string line;
    line.reserve(static_cast<size_t>(width) * 3);  // ━ is 3 bytes in UTF-8
    for (int i = 0; i < width; ++i) {
        line += "━";
    }
    return line;
}

}  // anonymous namespace

App::App() = default;

void App::finalize_system(Schedule schedule, std::unique_ptr<System> system,
                          std::vector<std::string> after, int priority,
                          std::string source) {
    SystemInfo info{
        .name = system->name(),
        .source = source.empty() ? current_source_ : std::move(source),
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

    // Schedule metrics (timing info from kernel)
    auto schedule_metrics = [&](Schedule schedule) -> std::string {
        switch (schedule) {
            case Schedule::Startup:
                return "once";
            case Schedule::First:
            case Schedule::PreUpdate:
            case Schedule::Update:
            case Schedule::PostUpdate:
            case Schedule::Last:
                return "every frame";
            case Schedule::FixedFirst:
            case Schedule::FixedPreUpdate:
            case Schedule::FixedUpdate:
            case Schedule::FixedPostUpdate:
            case Schedule::FixedLast:
                return std::to_string(static_cast<int>(std::round(1.0f / fixed_dt_))) + " Hz";
            case Schedule::Replication:
                return std::to_string(static_cast<int>(std::round(1.0f / replication_dt_))) + " Hz";
            case Schedule::Persistence:
                return std::to_string(static_cast<int>(std::round(1.0f / persistence_dt_))) + " Hz";
            case Schedule::Shutdown:
                return "once";
            default:
                return "";
        }
    };

    // Count total systems
    size_t total_systems = system_infos_.size();
    std::string line = terminal_line();

    std::cout << "\n";
    std::cout << DIM << line << RESET << "\n";
    std::cout << "  ASE Schedule Bootstrap " << DIM << "(" << total_systems << " systems)" << RESET << "\n";
    std::cout << DIM << line << RESET << "\n";

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

        std::string metrics = schedule_metrics(schedule);

        std::cout << "\n";
        std::cout << "  " << schedule_color(schedule) << "┌─ "
                  << schedule_name(schedule) << RESET;
        if (!metrics.empty()) {
            std::cout << " " << DIM << "(" << metrics << ")" << RESET;
        }
        std::cout << "\n";

        std::string prev_source;
        for (const auto* info : it->second) {
            // Empty line between different modules
            if (!prev_source.empty() && prev_source != info->source) {
                std::cout << "  " << schedule_color(schedule) << "│" << RESET << "\n";
            }
            prev_source = info->source;

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
    std::cout << DIM << line << RESET << "\n\n";
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

    // Build name -> info mapping for source lookup
    std::unordered_map<std::string, const SystemInfo*> info_map;
    for (const auto& info : system_infos_) {
        info_map[info.name] = &info;
    }

    // Count total systems
    size_t total = system_infos_.size();
    size_t current = 0;

    // ANSI colors for boot log
    constexpr const char* RESET = "\x1b[0m";
    constexpr const char* DIM = "\x1b[38;5;243m";
    constexpr const char* GREEN = "\x1b[38;5;71m";
    constexpr const char* CYAN = "\x1b[36m";

    // Call on_start() for ALL systems (initialization)
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto& system : systems) {
            ++current;
            const auto* info = info_map[system->name()];
            std::string source = info ? info->source : "";
            if (source.empty()) source = "unknown";

            // Call on_start first, then log (so system logs don't interleave)
            system->on_start(world_.registry());

            // Boot log with std::cout
            std::cout << DIM << "[" << timestamp() << "]" << RESET << " "
                      << DIM << "[Booting]" << RESET << " "
                      << CYAN << "[" << std::setw(3) << std::setfill('0') << current
                      << "/" << std::setw(3) << std::setfill('0') << total << "]" << RESET << " "
                      << GREEN << "[" << source << "]" << RESET << " "
                      << "[" << system->name() << "] Started\n";
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

    // Build name -> info mapping for source lookup
    std::unordered_map<std::string, const SystemInfo*> info_map;
    for (const auto& info : system_infos_) {
        info_map[info.name] = &info;
    }

    // Count total systems
    size_t total = system_infos_.size();
    size_t current = 0;

    // ANSI colors for shutdown log
    constexpr const char* RESET = "\x1b[0m";
    constexpr const char* DIM = "\x1b[38;5;243m";
    constexpr const char* YELLOW = "\x1b[33m";
    constexpr const char* RED = "\x1b[38;5;167m";

    // Call on_stop() for ALL systems (cleanup) in reverse order
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
            ++current;
            const auto* info = info_map[(*it)->name()];
            std::string source = info ? info->source : "";
            if (source.empty()) source = "unknown";

            // Call on_stop first, then log (so system logs don't interleave)
            (*it)->on_stop(world_.registry());

            // Shutdown log with std::cout
            std::cout << DIM << "[" << timestamp() << "]" << RESET << " "
                      << DIM << "[Shutdown]" << RESET << " "
                      << YELLOW << "[" << std::setw(3) << std::setfill('0') << current
                      << "/" << std::setw(3) << std::setfill('0') << total << "]" << RESET << " "
                      << RED << "[" << source << "]" << RESET << " "
                      << "[" << (*it)->name() << "] Stopped\n";
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
