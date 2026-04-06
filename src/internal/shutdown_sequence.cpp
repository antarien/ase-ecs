#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/terminal_utils.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_module.hpp>
#include <ase/log/colors.hpp>

#include <spdlog/sinks/base_sink.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ase::ecs::internal {

namespace {

// Queue sink that captures log messages during shutdown
template<typename Mutex>
class ShutdownQueueSink : public spdlog::sinks::base_sink<Mutex> {
public:
    struct LogEntry {
        spdlog::level::level_enum level;
        std::string payload;
    };

    std::vector<LogEntry>& entries() { return entries_; }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        entries_.push_back({msg.level, std::string(msg.payload.data(), msg.payload.size())});
    }

    void flush_() override {}

private:
    std::vector<LogEntry> entries_;
};

using ShutdownQueueSinkMt = ShutdownQueueSink<std::mutex>;

// Level colors and names (same as log_system.cpp)
const char* LEVEL_COLORS[] = {
    "\x1b[38;5;243m", // trace
    "\x1b[38;5;67m",  // debug
    "\x1b[38;5;71m",  // info
    "\x1b[38;5;179m", // warn
    "\x1b[38;5;167m", // error
    "\x1b[38;5;168m", // critical
};
const char* LEVEL_NAMES[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT"};

}  // anonymous namespace

void print_shutdown_sequence(SystemRegistry& registry, World& world,
                              const ShutdownConfig& config) {
    // Build name → info mapping for source lookup
    std::unordered_map<std::string, const SystemInfo*> info_map;
    for (const auto& info : registry.infos()) {
        info_map[info.name] = &info;
    }

    size_t total = registry.total_count();
    size_t current = 0;

    // Create queue sink to capture logs during shutdown
    auto queue_sink = std::make_shared<ShutdownQueueSinkMt>();

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

    // Header
    std::cout << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n" << std::flush;
    std::cout << "  ASE Shutdown Sequence " << ansi::DIM << "(" << total << " systems)" << ansi::RESET << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n" << std::flush;

    // Pre-calculate GLOBAL source totals (total systems per module/plugin)
    std::unordered_map<std::string, size_t> global_source_totals;
    for (const auto& info : registry.infos()) {
        global_source_totals[info.source]++;
    }

    // Track current index per source (for module-local counter, counting UP)
    std::unordered_map<std::string, size_t> source_current_idx;

    std::cout << "\n" << std::flush;
    std::cout << "  " << ansi::BLUE << "┌─ Shutdown" << ansi::RESET << " " << ansi::DIM << "(once)" << ansi::RESET << "\n" << std::flush;

    // Call on_stop() for ALL systems in reverse order
    std::string prev_source;
    for (auto& [schedule, systems] : registry.all_systems()) {
        for (auto it = systems.rbegin(); it != systems.rend(); ++it) {
            if (!*it) continue;  // Skip null entries

            ++current;
            const auto* info = info_map[(*it)->name()];
            std::string source = info ? info->source : "unknown";

            // Increment and get module-local index
            source_current_idx[source]++;
            size_t module_current = source_current_idx[source];
            size_t module_total = global_source_totals[source];
            // For shutdown (reverse), show remaining: module_total - module_current + 1
            size_t module_remaining = module_total - module_current + 1;

            // Empty line between module/plugin groups
            if (!prev_source.empty() && prev_source != source) {
                std::cout << "  " << ansi::BLUE << "│" << ansi::RESET << "\n" << std::flush;
            }
            prev_source = source;

            // Shutdown delay
            if (config.shutdown_delay_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(config.shutdown_delay_us));
            }

            // Get source color (works for both ase-* modules and ase-pl-* plugins)
            int src_color = log::get_module_color_code(source);

            // Build the line content
            // Count DOWN from total to 1 (shutdown is reverse order)
            size_t remaining = total - current + 1;
            std::ostringstream line_content;
            line_content << "  " << ansi::BLUE << "│" << ansi::RESET << " ";

            if (config.show_timestamps) {
                line_content << ansi::DIM << "[" << short_timestamp() << "]" << ansi::RESET << " ";
            }

            // Counter format: [module_remaining/module_total] [global_remaining/global_total]
            std::string version = info ? info->version : "";
            line_content << ansi::DIM << "[Down]" << ansi::RESET << " "
                         << ansi::CYAN << "["
                         << std::setfill('0') << std::setw(3) << module_remaining << "/"
                         << std::setfill('0') << std::setw(3) << module_total << "]"
                         << ansi::RESET << " "
                         << ansi::DIM << "["
                         << std::setfill('0') << std::setw(3) << remaining << "/"
                         << std::setfill('0') << std::setw(3) << total << "]"
                         << ansi::RESET << " "
                         << "\x1b[38;5;" << src_color << "m[" << source << "]" << ansi::RESET << " ";

            // Show version if available, or [!] warning if missing
            if (!version.empty()) {
                line_content << ansi::DIM << "[" << version << "]" << ansi::RESET << " ";
            } else {
                line_content << ansi::YELLOW << "[!]" << ansi::RESET << " ";
            }

            std::ostringstream line_suffix;
            line_suffix << ansi::WHITE << (*it)->name() << ansi::RESET;

            // Print [..] before on_stop
            std::cout << line_content.str() << ansi::YELLOW << "[..]" << ansi::RESET << " " << line_suffix.str() << std::flush;

            // Call on_stop
            (*it)->on_stop(world.registry());

            // Overwrite with [OK]
            std::cout << "\r\x1b[K" << line_content.str() << ansi::OK_GREEN << "[OK]" << ansi::RESET << " " << line_suffix.str() << "\n" << std::flush;
        }
    }

    // Footer
    std::cout << "\n" << std::flush;
    std::cout << ansi::DIM << line << ansi::RESET << "\n\n" << std::flush;

    // Replay queued logs to stdout (LogSystem may be stopped)
    if (!queue_sink->entries().empty()) {
        for (const auto& entry : queue_sink->entries()) {
            auto idx = static_cast<size_t>(entry.level);
            if (idx >= 6) idx = 5;

            std::cout << ansi::DIM << "[" << full_timestamp() << "]" << ansi::RESET << " "
                      << "[" << LEVEL_COLORS[idx] << LEVEL_NAMES[idx] << ansi::RESET << "] "
                      << "[ASE] [" << (world.registry().ctx().contains<ase::log::LogConfig>() ? world.registry().ctx().get<ase::log::LogConfig>().label : "SERVER") << "] " << entry.payload << "\n";
        }
        std::cout << std::flush;
    }

    // Note: We don't restore original sinks since the app is shutting down
    (void)original_sinks;
    (void)sinks_replaced;
}

}  // namespace ase::ecs::internal
