#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/terminal_utils.hpp>
#include <ase/log/log.hpp>
#include <ase/log/colors.hpp>

#include <spdlog/sinks/base_sink.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ase::ecs::internal {

namespace {

// Queue sink that captures log messages during boot
template<typename Mutex>
class QueueSink : public spdlog::sinks::base_sink<Mutex> {
public:
    struct LogEntry {
        spdlog::level::level_enum level;
        std::string payload;
    };

    std::vector<LogEntry>& entries() { return entries_; }
    void clear() { entries_.clear(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        entries_.push_back({msg.level, std::string(msg.payload.data(), msg.payload.size())});
    }

    void flush_() override {}

private:
    std::vector<LogEntry> entries_;
};

using QueueSinkMt = QueueSink<std::mutex>;

// Get tier color based on schedule tier name
const char* tier_color(const char* tier_name) {
    std::string_view tier(tier_name);
    if (tier == "Lifecycle") return ansi::BLUE;
    if (tier == "Frame") return ansi::CYAN;
    if (tier == "Kinetic") return ansi::GREEN;
    if (tier == "Reactive") return ansi::MAGENTA;
    if (tier == "Tactical") return ansi::YELLOW;
    if (tier == "Adaptive") return ansi::YELLOW;
    if (tier == "Cyclic") return ansi::RED;
    return ansi::WHITE;
}

// Format schedule metrics (hz or interval)
std::string format_metrics(Schedule schedule) {
    float hz = schedule_hz(schedule);
    if (hz == 0.0f) return "once";
    if (hz >= 60.0f) return "every frame";
    if (hz >= 1.0f) return std::to_string(static_cast<int>(hz)) + " Hz";
    float interval = schedule_interval(schedule);
    if (interval >= 3600.0f) return std::to_string(static_cast<int>(interval / 3600.0f)) + "h";
    if (interval >= 60.0f) return std::to_string(static_cast<int>(interval / 60.0f)) + "min";
    return std::to_string(static_cast<int>(interval)) + "s";
}

// All schedules in display order
const Schedule SCHEDULE_ORDER[] = {
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

constexpr size_t SCHEDULE_ORDER_COUNT = sizeof(SCHEDULE_ORDER) / sizeof(SCHEDULE_ORDER[0]);

}  // anonymous namespace

void print_boot_sequence(SystemRegistry& registry, World& world,
                         const BootLoggerConfig& config) {
    size_t total_systems = registry.total_count();
    size_t current_system = 0;
    std::string line = terminal_line();

    // Build name → system mapping for on_start calls
    std::unordered_map<std::string, System*> name_to_system;
    for (auto& [schedule, systems] : registry.all_systems()) {
        for (auto& system : systems) {
            if (system) {
                name_to_system[system->name()] = system.get();
            }
        }
    }

    // Create queue sink to capture logs during boot
    auto queue_sink = std::make_shared<QueueSinkMt>();
    std::vector<spdlog::sink_ptr> original_sinks;
    bool sinks_replaced = false;

    // Header
    std::cout << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n" << std::flush;
    std::cout << "  ASE Schedule Bootstrap " << ansi::DIM << "(" << total_systems << " systems)" << ansi::RESET << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n" << std::flush;

    // Pre-calculate GLOBAL source totals (total systems per module/plugin)
    std::unordered_map<std::string, size_t> global_source_totals;
    for (const auto& info : registry.infos()) {
        global_source_totals[info.source]++;
    }

    // Track current index per source (for module-local counter)
    std::unordered_map<std::string, size_t> source_current_idx;

    // Group system infos by schedule
    std::unordered_map<Schedule, std::vector<const SystemInfo*>> grouped;
    for (const auto& info : registry.infos()) {
        grouped[info.schedule].push_back(&info);
    }

    // Process schedules in order
    for (size_t sched_idx = 0; sched_idx < SCHEDULE_ORDER_COUNT; ++sched_idx) {
        Schedule schedule = SCHEDULE_ORDER[sched_idx];
        auto it = grouped.find(schedule);
        if (it == grouped.end() || it->second.empty()) {
            continue;
        }

        const char* sched_tier = schedule_tier(schedule);
        const char* sched_color = tier_color(sched_tier);
        std::string metrics = format_metrics(schedule);

        // Schedule header
        std::cout << "\n" << std::flush;
        std::cout << "  " << sched_color << "┌─ "
                  << schedule_name(schedule) << ansi::RESET;
        if (!metrics.empty()) {
            std::cout << " " << ansi::DIM << "(" << metrics << ")" << ansi::RESET;
        }
        std::cout << "\n" << std::flush;

        // Count systems per module
        std::unordered_map<std::string, size_t> module_counts;
        for (const auto* info_ptr : it->second) {
            module_counts[info_ptr->source]++;
        }

        std::string prev_source;
        size_t prev_module_count = 0;

        for (const auto* info : it->second) {
            // Empty line between module groups
            if (!prev_source.empty() && prev_source != info->source) {
                size_t curr_module_count = module_counts[info->source];
                if (prev_module_count > 1 || curr_module_count > 1) {
                    std::cout << "  " << sched_color << "│" << ansi::RESET << "\n" << std::flush;
                }
            }

            if (prev_source != info->source) {
                prev_module_count = module_counts[info->source];
            }
            prev_source = info->source;

            ++current_system;

            // Increment and get module-local index
            source_current_idx[info->source]++;
            size_t module_idx = source_current_idx[info->source];
            size_t module_total = global_source_totals[info->source];

            // Boot delay
            if (config.boot_delay_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(config.boot_delay_us));
            }

            // Get module color from log system catalog
            int mod_color = log::get_module_color_code(info->source);

            // Build line content
            std::ostringstream line_content;
            line_content << "  " << sched_color << "│" << ansi::RESET << " ";

            if (config.show_timestamps) {
                line_content << ansi::DIM << "[" << short_timestamp() << "]" << ansi::RESET << " ";
            }

            // Counter format: [module_idx/global_idx/module_total/global_total]
            line_content << ansi::DIM << "[Boot]" << ansi::RESET << " "
                         << ansi::CYAN << "["
                         << std::setfill('0') << std::setw(3) << module_idx << "/"
                         << std::setfill('0') << std::setw(3) << current_system << "/"
                         << std::setfill('0') << std::setw(3) << module_total << "/"
                         << std::setfill('0') << std::setw(3) << total_systems << "]"
                         << ansi::RESET << " "
                         << "\x1b[38;5;" << mod_color << "m[" << info->source << "]" << ansi::RESET << " ";

            // Show version if available, or [!] warning if missing
            if (!info->version.empty()) {
                line_content << ansi::DIM << "[" << info->version << "]" << ansi::RESET << " ";
            } else {
                line_content << ansi::YELLOW << "[!]" << ansi::RESET << " ";
            }

            // Build suffix with system name and dependencies
            std::ostringstream line_suffix;
            line_suffix << ansi::WHITE << info->name << ansi::RESET;

            if (config.show_dependencies && !info->run_after.empty()) {
                int term_width = get_terminal_width();
                size_t prefix_width = 4 + 13 + 7 + 10 + 5 + info->source.size() + 3 + info->name.size() + 4;
                size_t max_deps_width = (term_width > static_cast<int>(prefix_width + 10))
                    ? static_cast<size_t>(term_width) - prefix_width
                    : 40;

                std::string deps_str;
                size_t shown_count = 0;
                for (size_t i = 0; i < info->run_after.size(); ++i) {
                    std::string next = (i > 0 ? ", " : "") + info->run_after[i];
                    if (deps_str.size() + next.size() > max_deps_width - 12) {
                        size_t remaining = info->run_after.size() - shown_count;
                        if (remaining > 0) {
                            deps_str += " (+" + std::to_string(remaining) + " more)";
                        }
                        break;
                    }
                    deps_str += next;
                    shown_count++;
                }
                line_suffix << ansi::DIM << " → " << deps_str << ansi::RESET;
            }

            // Print [..] before on_start
            std::cout << line_content.str() << ansi::YELLOW << "[..]" << ansi::RESET << " " << line_suffix.str() << std::flush;

            // Call on_start
            auto* system = name_to_system[info->name];
            if (system) {
                system->on_start(world.registry());

                // Replace sinks after LogSystem starts
                if (!sinks_replaced && log::LogSystem::logger()) {
                    original_sinks = log::LogSystem::logger()->sinks();
                    log::LogSystem::logger()->sinks().clear();
                    log::LogSystem::logger()->sinks().push_back(queue_sink);
                    sinks_replaced = true;
                }
            }

            // Overwrite with [OK]
            std::cout << "\r\x1b[K" << line_content.str() << ansi::OK_GREEN << "[OK]" << ansi::RESET << " " << line_suffix.str() << "\n" << std::flush;
        }
    }

    // Footer
    std::cout << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n\n" << std::flush;

    // Restore original sinks
    if (sinks_replaced && log::LogSystem::logger()) {
        log::LogSystem::logger()->sinks().clear();
        for (auto& sink : original_sinks) {
            log::LogSystem::logger()->sinks().push_back(sink);
        }
    }

    // Replay queued logs
    if (log::LogSystem::logger()) {
        for (const auto& entry : queue_sink->entries()) {
            log::LogSystem::logger()->log(entry.level, "{}", entry.payload);
        }
    }
}

}  // namespace ase::ecs::internal
