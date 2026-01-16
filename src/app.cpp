#include <ase/ecs/app.hpp>
#include <ase/log/log.hpp>
#include <ase/log/colors.hpp>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/pattern_formatter.h>
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
#include <mutex>
#include <sys/ioctl.h>
#include <unistd.h>

namespace ase::ecs {

namespace {

// Queue sink that captures log messages during boot (stores raw payload for replay)
template<typename Mutex>
class QueueSink : public spdlog::sinks::base_sink<Mutex> {
public:
    struct LogEntry {
        spdlog::level::level_enum level;
        std::string payload;  // Raw message text
    };

    std::vector<LogEntry>& entries() { return entries_; }
    void clear() { entries_.clear(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Store raw payload for replay through original logger
        entries_.push_back({msg.level, std::string(msg.payload.data(), msg.payload.size())});
    }

    void flush_() override {}

private:
    std::vector<LogEntry> entries_;
};

using QueueSinkMt = QueueSink<std::mutex>;


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
    constexpr const char* OK_GREEN = "\x1b[38;5;71m";  // Same muted green as [INF] in logs

    // Boot delay for visual effect (microseconds)
    constexpr int BOOT_DELAY_US = 15000;  // 15ms per system

    // Schedule colors based on frequency tier
    auto schedule_color = [&](Schedule schedule) -> const char* {
        const char* tier = schedule_tier(schedule);
        if (std::string_view(tier) == "Lifecycle") return BLUE;
        if (std::string_view(tier) == "Frame") return CYAN;
        if (std::string_view(tier) == "Kinetic") return GREEN;
        if (std::string_view(tier) == "Reactive") return MAGENTA;
        if (std::string_view(tier) == "Tactical") return YELLOW;
        if (std::string_view(tier) == "Adaptive") return YELLOW;
        if (std::string_view(tier) == "Cyclic") return RED;
        return WHITE;
    };

    // Schedule metrics using schedule_hz()
    auto schedule_metrics = [&](Schedule schedule) -> std::string {
        float hz = schedule_hz(schedule);
        if (hz == 0.0f) return "once";
        if (hz >= 60.0f) return "every frame";
        if (hz >= 1.0f) return std::to_string(static_cast<int>(hz)) + " Hz";
        float interval = schedule_interval(schedule);
        if (interval >= 3600.0f) return std::to_string(static_cast<int>(interval / 3600.0f)) + "h";
        if (interval >= 60.0f) return std::to_string(static_cast<int>(interval / 60.0f)) + "min";
        return std::to_string(static_cast<int>(interval)) + "s";
    };

    // Short timestamp: MM:SS.mmm
    auto short_timestamp = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf{};
        localtime_r(&time, &tm_buf);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << tm_buf.tm_min << ":"
            << std::setfill('0') << std::setw(2) << tm_buf.tm_sec << "."
            << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    };

    // Count total systems
    size_t total_systems = system_infos_.size();
    size_t current_system = 0;
    std::string line = terminal_line();

    // Build name -> system mapping for on_start calls
    std::unordered_map<std::string, System*> name_to_system;
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto& system : systems) {
            name_to_system[system->name()] = system.get();
        }
    }

    // Create queue sink to capture logs during boot (stores raw payload)
    auto queue_sink = std::make_shared<QueueSinkMt>();

    // Store original sinks (will be filled after LogSystem starts)
    std::vector<spdlog::sink_ptr> original_sinks;
    bool sinks_replaced = false;

    std::cout << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n" << std::flush;
    std::cout << "  ASE Schedule Bootstrap " << DIM << "(" << total_systems << " systems)" << RESET << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n" << std::flush;

    // Define schedule order (all 44 schedules)
    static const std::vector<Schedule> schedule_order = {
        // Lifecycle
        Schedule::Initialization,
        Schedule::Configuration,
        // Frame (~60Hz)
        Schedule::Reception,
        Schedule::Ingestion,
        Schedule::Integration,
        Schedule::Production,
        Schedule::Conclusion,
        // Kinetic (30Hz)
        Schedule::Dynamics,
        Schedule::Kinematics,
        Schedule::Collision,
        // Reactive (20Hz)
        Schedule::Transmission,
        Schedule::Synchronization,
        // Tactical (10Hz)
        Schedule::Perception,
        Schedule::Reaction,
        Schedule::Navigation,
        Schedule::Evaluation,
        // Adaptive (5Hz)
        Schedule::Deliberation,
        Schedule::Aggregation,
        Schedule::Correlation,
        Schedule::Coordination,
        // Progressive (2Hz)
        Schedule::Modulation,
        Schedule::Regulation,
        Schedule::Adaptation,
        // Cyclic (1Hz)
        Schedule::Dissemination,
        Schedule::Preservation,
        Schedule::Observation,
        // Gradual (10s)
        Schedule::Accumulation,
        Schedule::Consolidation,
        // Incremental (1min)
        Schedule::Maintenance,
        Schedule::Reconciliation,
        // Ambient (5min)
        Schedule::Maturation,
        Schedule::Degradation,
        // Periodic (15min)
        Schedule::Regeneration,
        Schedule::Decomposition,
        // Epochal (1h)
        Schedule::Evolution,
        Schedule::Erosion,
        // Extended (6h)
        Schedule::Succession,
        Schedule::Transformation,
        // Diurnal (24h)
        Schedule::Culmination,
        Schedule::Renewal,
        // Lifecycle (shutdown)
        Schedule::Termination,
        Schedule::Finalization
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

        std::cout << "\n" << std::flush;
        std::cout << "  " << schedule_color(schedule) << "┌─ "
                  << schedule_name(schedule) << RESET;
        if (!metrics.empty()) {
            std::cout << " " << DIM << "(" << metrics << ")" << RESET;
        }
        std::cout << "\n" << std::flush;

        // Count systems per module in this schedule
        std::unordered_map<std::string, size_t> module_counts;
        for (const auto* info_ptr : it->second) {
            module_counts[info_ptr->source]++;
        }

        std::string prev_source;
        size_t prev_module_count = 0;
        for (const auto* info : it->second) {
            // Empty line between module groups if either previous or current has >1 system
            if (!prev_source.empty() && prev_source != info->source) {
                size_t curr_module_count = module_counts[info->source];
                if (prev_module_count > 1 || curr_module_count > 1) {
                    std::cout << "  " << schedule_color(schedule) << "│" << RESET << "\n" << std::flush;
                }
            }

            if (prev_source != info->source) {
                prev_module_count = module_counts[info->source];
            }
            prev_source = info->source;

            ++current_system;

            // Boot delay for visual effect
            std::this_thread::sleep_for(std::chrono::microseconds(BOOT_DELAY_US));

            // Build the line content (without status) for reuse
            // Get module color from SSOT catalog
            int module_color = log::get_module_color_code(info->source);

            std::ostringstream line_content;
            line_content << "  " << schedule_color(schedule) << "│" << RESET << " "
                         << DIM << "[" << short_timestamp() << "]" << RESET << " "
                         << DIM << "[Boot]" << RESET << " "
                         << CYAN << "[" << std::setfill('0') << std::setw(3) << current_system
                         << "/" << std::setfill('0') << std::setw(3) << total_systems << "]" << RESET << " "
                         << "\x1b[38;5;" << module_color << "m[" << info->source << "]" << RESET << " ";

            // Calculate available width for dependencies
            // Terminal width minus fixed parts: prefix + timestamp + [Boot] + [NNN/NNN] + [module] + [OK] + system name
            int term_width = get_terminal_width();
            size_t prefix_width = 4 + 13 + 7 + 10 + 5 + info->source.size() + 3 + info->name.size() + 4;  // +4 for " → "
            size_t max_deps_width = (term_width > static_cast<int>(prefix_width + 10))
                ? static_cast<size_t>(term_width) - prefix_width
                : 40;  // Fallback minimum

            std::ostringstream line_suffix;
            line_suffix << WHITE << info->name << RESET;
            if (!info->run_after.empty()) {
                // Build dependencies string with truncation
                std::string deps_str;
                size_t shown_count = 0;
                for (size_t i = 0; i < info->run_after.size(); ++i) {
                    std::string next = (i > 0 ? ", " : "") + info->run_after[i];
                    if (deps_str.size() + next.size() > max_deps_width - 12) {  // Reserve space for " (+N more)"
                        size_t remaining = info->run_after.size() - shown_count;
                        if (remaining > 0) {
                            deps_str += " (+" + std::to_string(remaining) + " more)";
                        }
                        break;
                    }
                    deps_str += next;
                    shown_count++;
                }
                line_suffix << DIM << " → " << deps_str << RESET;
            }

            // Print [ .. ] line before on_start
            std::cout << line_content.str() << YELLOW << "[..]" << RESET << " " << line_suffix.str() << std::flush;

            // Call on_start (logs go to queue after LogSystem starts)
            auto* system = name_to_system[info->name];
            if (system) {
                system->on_start(world_.registry());

                // After LogSystem starts, replace sinks with queue sink to capture all subsequent logs
                if (!sinks_replaced && log::LogSystem::logger()) {
                    original_sinks = log::LogSystem::logger()->sinks();
                    log::LogSystem::logger()->sinks().clear();
                    log::LogSystem::logger()->sinks().push_back(queue_sink);
                    sinks_replaced = true;
                }
            }

            // Overwrite with [OK] using \r, then \x1b[K to clear remaining chars
            std::cout << "\r\x1b[K" << line_content.str() << OK_GREEN << "[OK]" << RESET << " " << line_suffix.str() << "\n" << std::flush;
        }
    }

    std::cout << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n\n" << std::flush;

    // Restore original sinks
    if (sinks_replaced && log::LogSystem::logger()) {
        log::LogSystem::logger()->sinks().clear();
        for (auto& sink : original_sinks) {
            log::LogSystem::logger()->sinks().push_back(sink);
        }
    }

    // Replay queued logs through original logger (preserves formatting)
    if (log::LogSystem::logger()) {
        for (const auto& entry : queue_sink->entries()) {
            log::LogSystem::logger()->log(entry.level, "{}", entry.payload);
        }
    }
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

    // Print boot log (also calls on_start for each system)
    print_boot_log();

    // Run Initialization schedule
    run_schedule(Schedule::Initialization, 0.0f);

    running_.store(true);
    last_frame_time_ = Clock::now();
}

void App::shutdown() {
    // Run Finalization schedule
    run_schedule(Schedule::Finalization, 0.0f);

    // ANSI colors (same as boot)
    constexpr const char* RESET = "\x1b[0m";
    constexpr const char* DIM = "\x1b[38;5;243m";
    constexpr const char* CYAN = "\x1b[36m";
    constexpr const char* BLUE = "\x1b[34m";
    constexpr const char* YELLOW = "\x1b[33m";
    constexpr const char* WHITE = "\x1b[37m";
    constexpr const char* OK_GREEN = "\x1b[38;5;71m";  // Same muted green as [INF] in logs

    // Shutdown delay for visual effect (microseconds) - same as boot
    constexpr int SHUTDOWN_DELAY_US = 15000;  // 15ms per system

    // Short timestamp: MM:SS.mmm (same as boot)
    auto short_timestamp = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf{};
        localtime_r(&time, &tm_buf);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << tm_buf.tm_min << ":"
            << std::setfill('0') << std::setw(2) << tm_buf.tm_sec << "."
            << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    };

    // Build name -> info mapping for source lookup
    std::unordered_map<std::string, const SystemInfo*> info_map;
    for (const auto& info : system_infos_) {
        info_map[info.name] = &info;
    }

    // Count total systems
    size_t total = system_infos_.size();
    size_t current = 0;

    // Create queue sink to capture logs during shutdown
    auto queue_sink = std::make_shared<QueueSinkMt>();

    // Store original sinks and replace with queue sink
    std::vector<spdlog::sink_ptr> original_sinks;
    bool sinks_replaced = false;
    if (log::LogSystem::logger()) {
        original_sinks = log::LogSystem::logger()->sinks();
        log::LogSystem::logger()->sinks().clear();
        log::LogSystem::logger()->sinks().push_back(queue_sink);
        sinks_replaced = true;
    }

    std::string line = terminal_line();

    std::cout << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n" << std::flush;
    std::cout << "  ASE Shutdown Sequence " << DIM << "(" << total << " systems)" << RESET << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n" << std::flush;

    std::cout << "\n" << std::flush;
    std::cout << "  " << BLUE << "┌─ Shutdown" << RESET << " " << DIM << "(once)" << RESET << "\n" << std::flush;

    // Call on_stop() for ALL systems (cleanup) in reverse order
    std::string prev_source;
    for (auto& [schedule, systems] : schedule_systems_) {
        for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
            ++current;
            const auto* info = info_map[(*it)->name()];
            std::string source = info ? info->source : "";
            if (source.empty()) source = "unknown";

            // Empty line between module groups
            if (!prev_source.empty() && prev_source != source) {
                std::cout << "  " << BLUE << "│" << RESET << "\n" << std::flush;
            }
            prev_source = source;

            // Shutdown delay for visual effect
            std::this_thread::sleep_for(std::chrono::microseconds(SHUTDOWN_DELAY_US));

            // Get module color from SSOT catalog
            int module_color = log::get_module_color_code(source);

            // Build the line content (without status) for reuse
            // Count DOWN from total to 1 (shutdown is reverse order)
            size_t remaining = total - current + 1;
            std::ostringstream line_content;
            line_content << "  " << BLUE << "│" << RESET << " "
                         << DIM << "[" << short_timestamp() << "]" << RESET << " "
                         << DIM << "[Down]" << RESET << " "
                         << CYAN << "[" << std::setfill('0') << std::setw(3) << remaining
                         << "/" << std::setfill('0') << std::setw(3) << total << "]" << RESET << " "
                         << "\x1b[38;5;" << module_color << "m[" << source << "]" << RESET << " ";

            std::ostringstream line_suffix;
            line_suffix << WHITE << (*it)->name() << RESET;

            // Print [ .. ] line before on_stop
            std::cout << line_content.str() << YELLOW << "[..]" << RESET << " " << line_suffix.str() << std::flush;

            // Call on_stop
            (*it)->on_stop(world_.registry());

            // Overwrite with [OK] using \r, then \x1b[K to clear remaining chars
            std::cout << "\r\x1b[K" << line_content.str() << OK_GREEN << "[OK]" << RESET << " " << line_suffix.str() << "\n" << std::flush;
        }
    }

    std::cout << "\n" << std::flush;
    std::cout << DIM << line << RESET << "\n\n" << std::flush;

    // Replay queued logs - LogSystem is stopped, so print directly to stdout
    // Using same format as log_system.cpp: [timestamp] [LEVEL] [ASE] [SERVER] message
    if (!queue_sink->entries().empty()) {
        // Level colors (same as log_system.cpp)
        static const char* level_colors[] = {
            "\x1b[38;5;243m", // trace - dark gray
            "\x1b[38;5;67m",  // debug - muted blue
            "\x1b[38;5;71m",  // info - muted green
            "\x1b[38;5;179m", // warn - muted yellow
            "\x1b[38;5;167m", // error - muted red
            "\x1b[38;5;168m", // critical - muted magenta
        };
        static const char* level_names[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT"};

        // Full timestamp for detail logs
        auto full_timestamp = []() -> std::string {
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
        };

        for (const auto& entry : queue_sink->entries()) {
            auto idx = static_cast<size_t>(entry.level);
            if (idx >= 6) idx = 5;  // Clamp to valid range

            std::cout << DIM << "[" << full_timestamp() << "]" << RESET << " "
                      << "[" << level_colors[idx] << level_names[idx] << RESET << "] "
                      << "[ASE] [SERVER] " << entry.payload << "\n";
        }
        std::cout << std::flush;
    }

    running_.store(false);
}

void App::tick(float dt) {
    if (dt > max_frame_time_) {
        dt = max_frame_time_;
    }

    // =========================================================================
    // Frame (~60Hz) - Every tick
    // =========================================================================
    run_schedule(Schedule::Reception, dt);
    run_schedule(Schedule::Ingestion, dt);
    run_schedule(Schedule::Integration, dt);
    run_schedule(Schedule::Production, dt);

    // =========================================================================
    // Kinetic (30Hz) - Physics simulation
    // =========================================================================
    fixed_accumulator_ += dt;
    while (fixed_accumulator_ >= fixed_dt_) {
        run_schedule(Schedule::Dynamics, fixed_dt_);
        run_schedule(Schedule::Kinematics, fixed_dt_);
        run_schedule(Schedule::Collision, fixed_dt_);
        fixed_accumulator_ -= fixed_dt_;
    }

    // =========================================================================
    // Reactive (20Hz) - Network synchronization
    // =========================================================================
    replication_accumulator_ += dt;
    if (replication_accumulator_ >= replication_dt_) {
        run_schedule(Schedule::Transmission, replication_dt_);
        run_schedule(Schedule::Synchronization, replication_dt_);
        replication_accumulator_ -= replication_dt_;
    }

    // =========================================================================
    // Tactical (10Hz) - Fast AI and perception
    // =========================================================================
    tactical_accumulator_ += dt;
    if (tactical_accumulator_ >= tactical_dt_) {
        run_schedule(Schedule::Perception, tactical_dt_);
        run_schedule(Schedule::Reaction, tactical_dt_);
        run_schedule(Schedule::Navigation, tactical_dt_);
        run_schedule(Schedule::Evaluation, tactical_dt_);
        tactical_accumulator_ -= tactical_dt_;
    }

    // =========================================================================
    // Adaptive (5Hz) - AI planning and coordination
    // =========================================================================
    adaptive_accumulator_ += dt;
    if (adaptive_accumulator_ >= adaptive_dt_) {
        run_schedule(Schedule::Deliberation, adaptive_dt_);
        run_schedule(Schedule::Aggregation, adaptive_dt_);
        run_schedule(Schedule::Correlation, adaptive_dt_);
        run_schedule(Schedule::Coordination, adaptive_dt_);
        adaptive_accumulator_ -= adaptive_dt_;
    }

    // =========================================================================
    // Progressive (2Hz) - Slow adjustments
    // =========================================================================
    progressive_accumulator_ += dt;
    if (progressive_accumulator_ >= progressive_dt_) {
        run_schedule(Schedule::Modulation, progressive_dt_);
        run_schedule(Schedule::Regulation, progressive_dt_);
        run_schedule(Schedule::Adaptation, progressive_dt_);
        progressive_accumulator_ -= progressive_dt_;
    }

    // =========================================================================
    // Cyclic (1Hz) - Regular intervals
    // =========================================================================
    persistence_accumulator_ += dt;
    if (persistence_accumulator_ >= persistence_dt_) {
        run_schedule(Schedule::Dissemination, persistence_dt_);
        run_schedule(Schedule::Preservation, persistence_dt_);
        run_schedule(Schedule::Observation, persistence_dt_);
        persistence_accumulator_ -= persistence_dt_;
    }

    // =========================================================================
    // Gradual (10s) - Slow accumulation
    // =========================================================================
    gradual_accumulator_ += dt;
    if (gradual_accumulator_ >= gradual_dt_) {
        run_schedule(Schedule::Accumulation, gradual_dt_);
        run_schedule(Schedule::Consolidation, gradual_dt_);
        gradual_accumulator_ -= gradual_dt_;
    }

    // =========================================================================
    // Incremental (1min) - Maintenance tasks
    // =========================================================================
    incremental_accumulator_ += dt;
    if (incremental_accumulator_ >= incremental_dt_) {
        run_schedule(Schedule::Maintenance, incremental_dt_);
        run_schedule(Schedule::Reconciliation, incremental_dt_);
        incremental_accumulator_ -= incremental_dt_;
    }

    // =========================================================================
    // Ambient (5min) - Very slow processes
    // =========================================================================
    ambient_accumulator_ += dt;
    if (ambient_accumulator_ >= ambient_dt_) {
        run_schedule(Schedule::Maturation, ambient_dt_);
        run_schedule(Schedule::Degradation, ambient_dt_);
        ambient_accumulator_ -= ambient_dt_;
    }

    // =========================================================================
    // Periodic (15min) - Periodic processes
    // =========================================================================
    periodic_accumulator_ += dt;
    if (periodic_accumulator_ >= periodic_dt_) {
        run_schedule(Schedule::Regeneration, periodic_dt_);
        run_schedule(Schedule::Decomposition, periodic_dt_);
        periodic_accumulator_ -= periodic_dt_;
    }

    // =========================================================================
    // Epochal (1h) - Hourly processes
    // =========================================================================
    epochal_accumulator_ += dt;
    if (epochal_accumulator_ >= epochal_dt_) {
        run_schedule(Schedule::Evolution, epochal_dt_);
        run_schedule(Schedule::Erosion, epochal_dt_);
        epochal_accumulator_ -= epochal_dt_;
    }

    // =========================================================================
    // Extended (6h) - Quarter-day processes
    // =========================================================================
    extended_accumulator_ += dt;
    if (extended_accumulator_ >= extended_dt_) {
        run_schedule(Schedule::Succession, extended_dt_);
        run_schedule(Schedule::Transformation, extended_dt_);
        extended_accumulator_ -= extended_dt_;
    }

    // =========================================================================
    // Diurnal (24h) - Daily processes
    // =========================================================================
    diurnal_accumulator_ += dt;
    if (diurnal_accumulator_ >= diurnal_dt_) {
        run_schedule(Schedule::Culmination, diurnal_dt_);
        run_schedule(Schedule::Renewal, diurnal_dt_);
        diurnal_accumulator_ -= diurnal_dt_;
    }

    // =========================================================================
    // Frame (~60Hz) - End of frame
    // =========================================================================
    run_schedule(Schedule::Conclusion, dt);
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
