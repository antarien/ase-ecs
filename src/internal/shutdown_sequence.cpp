/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        shutdown_sequence.cpp
 * @brief       Shutdown sequence visualization with colored output
 * @description Draws the shutdown progress in reverse registration order.
 *              Like boot_logger.cpp this file is a TERMINAL RENDERER, not a
 *              log site: it writes an aligned, coloured table straight to the
 *              terminal, and routing it through ase::log would prefix every
 *              row with timestamp, level and category and break the columns.
 *
 *              Each step is announced by a "[..]" row that the "[OK]" row
 *              replaces once on_stop has returned. The replacement needs a
 *              terminal on stdout (terminal_redraw_supported); on a pipe the
 *              finished row is drawn alone.
 *
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @usecase     usecase:100356 - run the shutdown sequence
 * @actor       Entity Registry Steward (ENTIREGSTE)
 * @created     2026-02-01
 * @modified    2026-11-05
 * @version     1.2.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/terminal_utils.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // the capture bracket around the shutdown table
#include <ase/log/log_config.hpp>     // LogConfig::label of the replayed lines
#include <ase/log/colors.hpp>

#include <ase/platform/sleep.hpp>
#include <ase/utils/clock.hpp>

#include <cstdio>
#include <string>
#include <ase/containers/hash_map.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

namespace {

// The log lines of the sequence run through the bracket log::capture_begin/count/entry/end:
// the sinks belong to the logger, so re-routing them belongs in ase-log. No type of the log
// library crosses the interface - a small integer for the level, the text through a buffer
// of this file.

// Buffer for ONE replayed log line. The bracket returns the full length even when it exceeds
// what fits here; the loop below compares both and appends a marker instead of printing a
// shortened line as if it were complete.
constexpr uint32_t CAPTURE_TEXT_MAX = 1024;

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

// Counters are drawn zero-padded to three digits so the columns line up with the
// boot view. A size_t prints in at most 20 digits, so this buffer holds any value
// the padding could ever be applied to: the format WIDENS short numbers, it never
// truncates long ones, and the line therefore stays honest for a tier with more
// than 999 systems.
constexpr size_t COUNTER_BUFFER_BYTES = 24;

std::string padded_counter(size_t value) {
    char buffer[COUNTER_BUFFER_BYTES];
    std::snprintf(buffer, sizeof(buffer), "%03zu", value);
    return std::string(buffer);
}

}  // anonymous namespace

void print_shutdown_sequence(SystemRegistry& registry, World& world,
                              const ShutdownConfig& config) {
    // Build name → info mapping for source lookup
    ase::containers::HashMap<std::string, const SystemInfo*> info_map;
    for (const auto& info : registry.infos()) {
        info_map[info.name] = &info;
    }

    size_t total = registry.total_count();
    size_t current = 0;

    // Measured once for the whole table: stdout does not change between two rows.
    const bool redraw = terminal_redraw_supported();

    // THE ONE LINE THAT STAYS IN THE LOG FILE - and it stands HERE on purpose, before the
    // redirection. Everything from capture_begin() on draws through write_terminal, because
    // the table REWRITES its rows and carries ANSI sequences and a fixed column form; sent
    // through a log sink that form would no longer be guaranteed (timestamp, level prefix,
    // the sink's line breaks). THE TERMINAL TRANSPORT IS THEREFORE LEFT AS IT IS - nothing
    // is removed and nothing re-routed, ONE line is added in front.
    //
    // WHY IT IS NEEDED: without it a clean shutdown leaves NO trace in the log file. A tier
    // that was shut down properly and one torn away mid-tick look identical there - both end
    // abruptly in their last working line. On 2026-08-28 none of the five tier logs could
    // tell whether its tier had stopped cleanly; the table that would have answered it had
    // gone with the console window.
    //
    // INF, not DBG: the per-system on_stop teardown logs on DBG and is therefore invisible in
    // normal operation (`--log +ERR +WRN +INF`) - measured 0 DBG lines in engine, dist and
    // replica. A marker that only appears with debug switched on does not answer the
    // question. Its counterpart at startup is INF as well.
    //
    // NO CLOSING LINE IN THE TIER SERVER, and that is no omission: there capture_end(false)
    // deliberately does NOT restore the sinks (the process ends with _exit right after), and
    // a line after it would go nowhere. This marker proves the ENTRY into teardown - that
    // App::shutdown() was reached instead of a hard death. The full course stays with the
    // table, where it belongs: on the terminal.
    // An embedded host (ShutdownConfig::restore_log_sinks) gets its sinks back at the end;
    // there a line after teardown also reaches the file and the callback.
    log::info("[Shutdown] sequence entered ({} systems)", total);

    // AND ONTO THE DISK AT ONCE. Without this flush the line exists but never reaches the
    // file: spdlog buffers, the next statement REPLACES the sinks with capture_begin(), and
    // the process ends with _exit(0) without a single destructor. Nobody empties the buffer
    // of the old sink after that.
    //
    // MEASURED on 2026-08-28: the marker was in the binary (strings: 1 hit, build 11:54:10
    // newer than the source), the category filter let it through (the log carried DBG lines
    // of several sources), the signal chain up to the shutdown path was intact (SIGTERM via
    // signalfd, replayed with the mask of a real tier) - and still it stood zero times in
    // logs/dist-9080.log after two DIST stops. Four green partial checks and an empty
    // result: the buffer was the only place left where the line could vanish.
    log::flush();

    // Redirect log output so that no line can tear the progress table below apart.
    // The return value is not checked: false means "there is no logger yet", and then there
    // is nothing to redirect either - the table runs the same in both cases.
    (void)log::capture_begin();

    std::string line = terminal_line();

    // Header. Same channel and same reason as boot_logger.cpp: this file replaces the
    // ase::log sinks for the duration of the sequence and rewrites its own lines, so it
    // draws through write_terminal instead of logging.
    std::string header = "\n";
    header += ansi::DIM;
    header += line;
    header += ansi::RESET;
    header += "\n  ASE Shutdown Sequence ";
    header += ansi::DIM;
    header += "(";
    header += std::to_string(total);
    header += total == 1 ? " system)" : " systems)";
    header += ansi::RESET;
    header += "\n";
    // ONE rule per table, above its title - the same form as the boot table.
    draw_terminal(config.render_terminal_table, header);

    // Pre-calculate GLOBAL source totals (total systems per module/plugin)
    ase::containers::HashMap<std::string, size_t> global_source_totals;
    for (const auto& info : registry.infos()) {
        global_source_totals[info.source]++;
    }

    // Track current index per source (for module-local counter, counting UP)
    ase::containers::HashMap<std::string, size_t> source_current_idx;

    std::string group_header = "\n  ";
    group_header += ansi::BLUE;
    group_header += "┌─ Shutdown";
    group_header += ansi::RESET;
    group_header += " ";
    group_header += ansi::DIM;
    group_header += "(once)";
    group_header += ansi::RESET;
    group_header += "\n";
    draw_terminal(config.render_terminal_table, group_header);

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
                std::string gap = "  ";
                gap += ansi::BLUE;
                gap += "│";
                gap += ansi::RESET;
                gap += "\n";
                draw_terminal(config.render_terminal_table, gap);
            }
            prev_source = source;

            // Shutdown delay. The `> 0` guard is load-bearing, same reason as in
            // boot_logger.cpp: shutdown_delay_us is a signed int
            // (its declaration in shutdown_sequence.hpp) and sleep_nanos takes an unsigned span, so a
            // negative value would wrap into a sleep no operator would sit through.
            if (config.shutdown_delay_us > 0) {
                ase::platform::sleep_nanos(static_cast<uint64_t>(config.shutdown_delay_us) *
                                           static_cast<uint64_t>(ase::utils::NANOS_PER_MICRO));
            }

            // Get source color (works for both ase-* modules and ase-pl-* plugins)
            int src_color = log::get_module_color_code(source.c_str());

            // Build the line content
            // Count DOWN from total to 1 (shutdown is reverse order)
            size_t remaining = total - current + 1;
            std::string line_content;
            line_content += "  ";
            line_content += ansi::BLUE;
            line_content += "│";
            line_content += ansi::RESET;
            line_content += " ";

            if (config.show_timestamps) {
                line_content += ansi::DIM;
                line_content += "[";
                line_content += short_timestamp();
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";
            }

            // Counter format: [module_remaining/module_total] [global_remaining/global_total]
            std::string version = info ? info->version : "";
            line_content += ansi::DIM;
            line_content += "[Down]";
            line_content += ansi::RESET;
            line_content += " ";
            line_content += ansi::CYAN;
            line_content += "[";
            line_content += padded_counter(module_remaining);
            line_content += "/";
            line_content += padded_counter(module_total);
            line_content += "]";
            line_content += ansi::RESET;
            line_content += " ";
            line_content += ansi::DIM;
            line_content += "[";
            line_content += padded_counter(remaining);
            line_content += "/";
            line_content += padded_counter(total);
            line_content += "]";
            line_content += ansi::RESET;
            line_content += " ";
            line_content += "\x1b[38;5;";
            line_content += std::to_string(src_color);
            line_content += "m[";
            line_content += source;
            line_content += "]";
            line_content += ansi::RESET;
            line_content += " ";

            // Show version if available, or [!] warning if missing
            if (!version.empty()) {
                line_content += ansi::DIM;
                line_content += "[";
                line_content += version;
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";
            } else {
                line_content += ansi::YELLOW;
                line_content += "[!]";
                line_content += ansi::RESET;
                line_content += " ";
            }

            std::string line_suffix;
            line_suffix += ansi::WHITE;
            line_suffix += (*it)->name();
            line_suffix += ansi::RESET;

            // Print [..] before on_stop. Flushed on its own - it announces a step that has
            // not run yet, and the row below replaces exactly this one. On a pipe nothing can
            // replace it, so the announcement is not drawn there at all.
            if (redraw) {
                std::string pending = line_content;
                pending += ansi::YELLOW;
                pending += "[..]";
                pending += ansi::RESET;
                pending += " ";
                pending += line_suffix;
                draw_terminal(config.render_terminal_table, pending);
            }

            // Call on_stop
            (*it)->on_stop(world.registry());

            // [OK] row; on a terminal it first takes back the [..] row drawn above.
            std::string done;
            if (redraw) {
                done += ansi::ERASE_ROW;
            }
            done += line_content;
            done += ansi::OK_GREEN;
            done += "[OK]";
            done += ansi::RESET;
            done += " ";
            done += line_suffix;
            done += "\n";
            draw_terminal(config.render_terminal_table, done);
        }
    }

    // Every system has stopped; the App still stands. Its lines are still captured and replayed
    // below with all the others.
    if (config.after_all_stopped != nullptr) {
        config.after_all_stopped(config.after_all_stopped_user);
    }

    // No footer rule: the table ends with its last row; the rule above the next title
    // separates it from whatever follows.

    // THE EMBEDDED HOST GETS ITS SINKS BACK - and with them the lines from on_stop.
    // capture_replay plays them through exactly the sinks that were active at capture_begin
    // (in the Vivarium client: callback and file from init_tui_standalone), capture_end(true)
    // hangs those sinks back in. The host lives on after teardown; every later line -
    // unloading the plugins, an error during teardown, the next restart - needs them.
    if (config.restore_log_sinks) {
        (void)log::capture_replay();
        log::capture_end(true);
        return;
    }

    // Replay queued logs to stdout (LogSystem may be stopped)
    const uint32_t captured = log::capture_count();
    if (captured > 0u) {
        char    text[CAPTURE_TEXT_MAX];
        uint8_t level = 0;

        for (uint32_t entry_idx = 0u; entry_idx < captured; ++entry_idx) {
            const uint32_t full = log::capture_entry(entry_idx, level, text, CAPTURE_TEXT_MAX);
            const size_t   idx  = static_cast<size_t>(level);  // already bounded to 0..5

            std::string replay = ansi::DIM;
            replay += "[";
            replay += full_timestamp();
            replay += "]";
            replay += ansi::RESET;
            replay += " [";
            replay += LEVEL_COLORS[idx];
            replay += LEVEL_NAMES[idx];
            replay += ansi::RESET;
            replay += "] [ASE] [";
            replay += (world.registry().ctx().contains<ase::log::LogConfig>()
                           ? world.registry().ctx().get<ase::log::LogConfig>().label
                           : "SERVER");
            replay += "] ";
            replay += text;
            if (full >= CAPTURE_TEXT_MAX) {
                // Made visible instead of kept quiet: the bracket reports the full length, so
                // a shortened line cannot pass here as a complete one.
                replay += ansi::DIM;
                replay += " [truncated]";
                replay += ansi::RESET;
            }
            replay += "\n";
            write_terminal(replay);
        }
        // One flush for the whole replay, as before: the loop above deliberately did not
        // flush per line.
        flush_terminal();
    }

    // In the tier server the sinks are deliberately NOT restored: the process ends here, and
    // the bracket has already replayed its content onto the terminal above. That is exactly
    // why capture_end takes an argument - boot_logger.cpp and the host branch above call the
    // same function with true.
    log::capture_end(false);
}

}  // namespace ase::ecs::internal
