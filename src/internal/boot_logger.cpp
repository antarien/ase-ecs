/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        boot_logger.cpp
 * @design      DSGN_021
 * @brief       Boot sequence visualization with colored output
 * @description Draws the startup progress grouped by schedule, with module
 *              colours, timestamps and dependency chains.
 *
 *              THIS FILE IS THE CONSOLE SINK DURING BOOT, and that is why it
 *              writes to the terminal directly. It clears the ase::log sinks,
 *              installs its own queue sink so no log line can interleave with
 *              the table it is drawing, and restores the original sinks when
 *              the boot block ends. Sending its own output through ase::log
 *              would feed it into the sink this file just installed - and
 *              would prefix an aligned, box-drawn table with timestamp, level
 *              and category. It is a terminal RENDERER, not a log site.
 *
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-02-01
 * @modified    2026-08-20
 * @version     1.0.0
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

#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/terminal_utils.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // die Capture-Klammer um die Boot-Tabelle
#include <ase/log/colors.hpp>

#include <cstdio>
#include <ase/utils/clock.hpp>
#include <string>
#include <ase/utils/strops.hpp>
#include <ase/platform/sleep.hpp>
#include <ase/containers/hash_map.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

namespace {

// A system whose on_start takes longer than this gets its duration printed next to the
// [OK]. Below it the number would be noise on every line of a fast boot.
constexpr int64_t BOOT_SLOW_SYSTEM_US = 100;

// Room for the millisecond figure written with "%g": at most six significant digits, a
// decimal point, an exponent and the terminator. 32 is far above the worst case.
constexpr size_t MS_BUFFER_BYTES = 32;

// Comparison bound for a schedule tier name. The longest in use is "Lifecycle" (9 bytes);
// the value leaves room for a longer tier without ever running off the end of a string
// that carries no terminator.
constexpr uint32_t TIER_NAME_MAX = 32;

// Hier stand eine Queue-Senke, die von der Logbibliothek erbte und den Senkenvektor des
// Loggers von Hand austauschte. shutdown_sequence.cpp trug dieselbe Klasse ein zweites Mal.
// Beide sind seit 2026-08-20 durch die Klammer log::capture_begin/replay/end ersetzt: die
// Senken gehoeren dem Logger, also gehoert das Umhaengen in ase-log.

// Get tier color based on schedule tier name.
//
// The comparisons run through ase::utils::str_equal, which is BOUNDED: it stops after
// TIER_NAME_MAX bytes rather than walking until it finds a NUL. The view this replaced was
// unbounded in the same way std::strlen is, and the argument arrives here as a bare
// const char* from schedule_tier() - the longest name in use is "Lifecycle" at 9 bytes, so
// the bound never truncates a comparison, it only removes the unbounded case.
const char* tier_color(const char* tier_name) {
    if (ase::utils::str_equal(tier_name, "Lifecycle", TIER_NAME_MAX)) return ansi::BLUE;
    if (ase::utils::str_equal(tier_name, "Frame", TIER_NAME_MAX))     return ansi::CYAN;
    if (ase::utils::str_equal(tier_name, "Kinetic", TIER_NAME_MAX))   return ansi::GREEN;
    if (ase::utils::str_equal(tier_name, "Reactive", TIER_NAME_MAX))  return ansi::MAGENTA;
    if (ase::utils::str_equal(tier_name, "Tactical", TIER_NAME_MAX))  return ansi::YELLOW;
    if (ase::utils::str_equal(tier_name, "Adaptive", TIER_NAME_MAX))  return ansi::YELLOW;
    if (ase::utils::str_equal(tier_name, "Cyclic", TIER_NAME_MAX))    return ansi::RED;
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

// Counters are drawn zero-padded to three digits so the columns line up with the
// shutdown view. A size_t prints in at most 20 digits, so this buffer holds any
// value the padding could ever be applied to: the format WIDENS short numbers, it
// never truncates long ones, and the line therefore stays honest for a tier with
// more than 999 systems.
constexpr size_t COUNTER_BUFFER_BYTES = 24;

std::string padded_counter(size_t value) {
    char buffer[COUNTER_BUFFER_BYTES];
    std::snprintf(buffer, sizeof(buffer), "%03zu", value);
    return std::string(buffer);
}

}  // anonymous namespace

void print_boot_sequence(SystemRegistry& registry, World& world,
                         const BootLoggerConfig& config) {
    size_t total_systems = registry.total_count();
    size_t current_system = 0;
    std::string line = terminal_line();

    // O(1) lookups via persistent maps in SystemRegistry (no temp-maps!)
    // name_to_system: registry.find_system(name)
    // source_totals:  registry.source_total(source)

    // No local sink swap / queue_sink needed anymore. ase-log drives the
    // "silent-during-boot, flushed-after-boot" behaviour itself: the logger
    // carries only file + HTTP-ring + counting + capture-ring during the
    // boot block (console sink is withheld), so log lines from any system's
    // on_start cannot reach stdout until finalize_logger_after_boot runs
    // below. No interleaving into this progress table.

    // Header. Built whole and flushed once - the stream form flushed after each of the
    // four lines, which put the same bytes on the terminal through four calls.
    std::string header = "\n";
    header += ansi::DIM;
    header += line;
    header += ansi::RESET;
    header += "\n  ASE Schedule Bootstrap ";
    header += ansi::DIM;
    header += "(";
    header += std::to_string(total_systems);
    header += " systems)";
    header += ansi::RESET;
    header += "\n";
    header += ansi::DIM;
    header += line;
    header += ansi::RESET;
    header += "\n";
    write_terminal(header);
    flush_terminal();

    // Track current index per source (for module-local counter)
    ase::containers::HashMap<std::string, size_t> source_current_idx;

    // Use persistent schedule_info_indices_ from SystemRegistry (no temp-map!)
    const auto& by_schedule = registry.infos_by_schedule();
    const auto& all_infos = registry.infos();

    // Process schedules in order
    for (size_t sched_idx = 0; sched_idx < SCHEDULE_ORDER_COUNT; ++sched_idx) {
        Schedule schedule = SCHEDULE_ORDER[sched_idx];
        auto it = by_schedule.find(schedule);
        if (it == by_schedule.end() || it->second.empty()) {
            continue;
        }

        const char* sched_tier = schedule_tier(schedule);
        const char* sched_color = tier_color(sched_tier);
        std::string metrics = format_metrics(schedule);

        // Schedule header
        std::string sched_header = "\n  ";
        sched_header += sched_color;
        sched_header += "┌─ ";
        sched_header += schedule_name(schedule);
        sched_header += ansi::RESET;
        if (!metrics.empty()) {
            sched_header += " ";
            sched_header += ansi::DIM;
            sched_header += "(";
            sched_header += metrics;
            sched_header += ")";
            sched_header += ansi::RESET;
        }
        sched_header += "\n";
        write_terminal(sched_header);
        flush_terminal();

        std::string prev_source;
        size_t prev_module_count = 0;

        for (size_t idx : it->second) {
            const auto* info = &all_infos[idx];
            // Empty line between module groups
            if (!prev_source.empty() && prev_source != info->source) {
                size_t curr_module_count = registry.schedule_source_count(schedule, info->source);
                if (prev_module_count > 1 or curr_module_count > 1) {
                    std::string gap = "  ";
                    gap += sched_color;
                    gap += "│";
                    gap += ansi::RESET;
                    gap += "\n";
                    write_terminal(gap);
                    flush_terminal();
                }
            }

            if (prev_source != info->source) {
                prev_module_count = registry.schedule_source_count(schedule, info->source);
            }
            prev_source = info->source;

            ++current_system;

            // Increment and get module-local index
            source_current_idx[info->source]++;
            size_t module_idx = source_current_idx[info->source];
            size_t module_total = registry.source_total(info->source);

            // Boot delay. The `> 0` guard is LOad-BEARING and not a nicety: boot_delay_us is
            // a signed int (its declaration in boot_logger.hpp), sleep_nanos takes an unsigned span, and a
            // negative value would wrap into a sleep of some three hundred years. The
            // stream form this replaced tolerated it - sleep_for returns at once on a
            // negative duration - so the guard is what carries that behaviour across.
            if (config.boot_delay_us > 0) {
                ase::platform::sleep_nanos(static_cast<uint64_t>(config.boot_delay_us) *
                                           static_cast<uint64_t>(ase::utils::NANOS_PER_MICRO));
            }

            // Get module color from log system catalog
            int mod_color = log::get_module_color_code(info->source.c_str());

            // Build line content
            std::string line_content;
            line_content += "  ";
            line_content += sched_color;
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

            // Counter format: [module_idx/module_total] [global_idx/global_total]
            line_content += ansi::DIM;
            line_content += "[Boot]";
            line_content += ansi::RESET;
            line_content += " ";
            line_content += ansi::CYAN;
            line_content += "[";
            line_content += padded_counter(module_idx);
            line_content += "/";
            line_content += padded_counter(module_total);
            line_content += "]";
            line_content += ansi::RESET;
            line_content += " ";
            line_content += ansi::DIM;
            line_content += "[";
            line_content += padded_counter(current_system);
            line_content += "/";
            line_content += padded_counter(total_systems);
            line_content += "]";
            line_content += ansi::RESET;
            line_content += " ";

            // Version before OK/status
            if (!info->version.empty()) {
                line_content += ansi::DIM;
                line_content += "[";
                line_content += info->version;
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";
            } else {
                line_content += ansi::YELLOW;
                line_content += "[!]";
                line_content += ansi::RESET;
                line_content += " ";
            }

            // Print [..] before on_start, then [source] SystemName. This one MUST be
            // flushed on its own: it announces a step that has not run yet, and the
            // overwrite below replaces exactly this line.
            std::string pending = line_content;
            pending += ansi::YELLOW;
            pending += "[..]";
            pending += ansi::RESET;
            pending += " \x1b[38;5;";
            pending += std::to_string(mod_color);
            pending += "m[";
            pending += info->source;
            pending += "]";
            pending += ansi::RESET;
            pending += " ";
            pending += ansi::WHITE;
            pending += info->name;
            pending += ansi::RESET;
            write_terminal(pending);
            flush_terminal();

            // Call on_start. The logger stays silent on stdout here because
            // no console sink is attached yet — that happens in
            // finalize_logger_after_boot (called below after the footer).
            auto* system = registry.find_system(info->name);
            if (system) {
                system->on_start(world.registry());
            }

            // Overwrite with [OK] [source] SystemName. The carriage return plus erase
            // sequence rewrites the [..] line printed above - the one thing a log backend
            // cannot do, and the reason this file draws instead of logging.
            std::string done = "\r\x1b[K";
            done += line_content;
            done += ansi::OK_GREEN;
            done += "[OK]";
            done += ansi::RESET;
            done += " \x1b[38;5;";
            done += std::to_string(mod_color);
            done += "m[";
            done += info->source;
            done += "]";
            done += ansi::RESET;
            done += " ";
            done += ansi::WHITE;
            done += info->name;
            done += ansi::RESET;
            done += "\n";
            write_terminal(done);
            flush_terminal();

            // Dependencies on separate sub-line (all listed, no truncation)
            if (config.show_dependencies and !info->run_after.empty()) {
                std::string deps = "  ";
                deps += sched_color;
                deps += "│";
                deps += ansi::RESET;
                deps += "   ";
                deps += ansi::DIM;
                deps += "→ ";
                for (size_t i = 0; i < info->run_after.size(); ++i) {
                    if (i > 0) deps += ", ";
                    deps += info->run_after[i];
                }
                deps += ansi::RESET;
                deps += "\n";
                write_terminal(deps);
                flush_terminal();
            }
        }
    }

    // Footer
    std::string footer = "\n";
    if (!registry.has_pending()) {
        footer += ansi::DIM;
        footer += line;
        footer += ansi::RESET;
        footer += "\n\n";
    }
    write_terminal(footer);
    flush_terminal();

    // Finalize the logger: attach the console sink (parked in ase-log during
    // LogSystem::on_start), replay the capture-ring — which holds every log
    // line produced by Kernel::build, KernelEnvLdrSystem, KernelCmdSystem
    // AND every system's on_start during the boot block above — into all
    // sinks (console + file + HTTP-ring + counting), then detach + drop
    // the capture ring. Every entry lands both on stdout and in
    // logs/{server}-{port}.log with correct [LABEL] + uppercase level format.
    log::finalize_logger_after_boot();
}

void boot_pending_systems(SystemRegistry& registry, World& world,
                          const BootLoggerConfig& /*config*/) {
    // Drain pending entries and register them into main lists
    // (deferred during boot to prevent iterator invalidation in print_boot_sequence)
    auto pending = registry.drain_pending();
    if (pending.empty()) { return; }

    ase::containers::HashSet<std::string> pending_names;
    pending_names.reserve(pending.size());
    for (auto& entry : pending) {
        pending_names.insert(entry.name);
        // Now safe to add to main lists (boot iteration is done)
        registry.add_deferred(entry.schedule, std::move(entry.owned_system),
                              entry.info);
    }

    size_t total = registry.total_count();
    size_t pending_count = pending_names.size();
    size_t booted = 0;

    // Count L3 modules vs L4 plugins
    size_t l3_count = 0, l4_count = 0;
    for (const auto& name : pending_names) {
        const auto* si = registry.find_info(name);
        if (si && si->source.substr(0, 6) == "ase-pl") ++l4_count;
        else ++l3_count;
    }

    std::string line = terminal_line();

    // Logausgabe umleiten, damit eine Zeile aus irgendeinem on_start die [..]→[OK]-Zeilen
    // nicht zerreisst. Diese Stelle liegt NACH finalize_logger_after_boot (weiter oben in
    // dieser Datei): davor haelt ase-log die Ausgabe ohnehin zurueck, ab dort nicht mehr —
    // deshalb ist die Klammer hier noetig und nicht bloss uebernommener Altbestand.
    (void)log::capture_begin();

    // Track per-source counters (same pattern as print_boot_sequence)
    ase::containers::HashMap<std::string, size_t> source_current_idx;

    // Boot helper: boot all pending systems matching layer filter
    auto boot_layer = [&](bool plugins_only, const char* section_label, size_t section_count) {
        std::string section = "\n";
        section += ansi::DIM;
        section += line;
        section += ansi::RESET;
        section += "\n  ";
        section += section_label;
        section += " ";
        section += ansi::DIM;
        section += "(";
        section += std::to_string(section_count);
        section += ")";
        section += ansi::RESET;
        section += "\n";
        section += ansi::DIM;
        section += line;
        section += ansi::RESET;
        section += "\n\n";
        write_terminal(section);
        flush_terminal();

        Schedule prev_schedule = static_cast<Schedule>(-1);

        for (size_t sched_idx = 0; sched_idx < SCHEDULE_ORDER_COUNT; ++sched_idx) {
            Schedule schedule = SCHEDULE_ORDER[sched_idx];
            auto& systems = registry.systems_for(schedule);
            bool header_printed = false;
            const char* sched_color = tier_color(schedule_tier(schedule));

            for (auto& system : systems) {
                if (!system) { continue; }

                auto it = pending_names.find(system->name());
                if (it == pending_names.end()) { continue; }

                const SystemInfo* info = registry.find_info(system->name());
                const std::string& source = info ? info->source : std::string("unknown");

                // Layer filter: ase-pl-* = plugin, everything else = module
                bool is_plugin = (source.size() > 6 && source.substr(0, 6) == "ase-pl");
                if (is_plugin != plugins_only) { continue; }

                // Schedule header
                if (!header_printed) {
                    std::string metrics = format_metrics(schedule);
                    std::string pending_header;
                    if (prev_schedule != static_cast<Schedule>(-1)) {
                        pending_header += "\n";
                    }
                    pending_header += "  ";
                    pending_header += sched_color;
                    pending_header += "┌─ ";
                    pending_header += schedule_name(schedule);
                    pending_header += ansi::RESET;
                    if (!metrics.empty()) {
                        pending_header += " ";
                        pending_header += ansi::DIM;
                        pending_header += "(";
                        pending_header += metrics;
                        pending_header += ")";
                        pending_header += ansi::RESET;
                    }
                    pending_header += "\n";
                    write_terminal(pending_header);
                    flush_terminal();
                    header_printed = true;
                    prev_schedule = schedule;
                }

                ++booted;
                source_current_idx[source]++;
                size_t module_idx = source_current_idx[source];
                size_t module_total = registry.source_total(source);
                int mod_color = log::get_module_color_code(source.c_str());

                std::string line_content;
                line_content += "  ";
                line_content += sched_color;
                line_content += "│";
                line_content += ansi::RESET;
                line_content += " ";
                line_content += ansi::DIM;
                line_content += "[";
                line_content += short_timestamp();
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";
                line_content += ansi::DIM;
                line_content += "[Boot]";
                line_content += ansi::RESET;
                line_content += " ";
                line_content += ansi::CYAN;
                line_content += "[";
                line_content += padded_counter(module_idx);
                line_content += "/";
                line_content += padded_counter(module_total);
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";
                line_content += ansi::DIM;
                line_content += "[";
                line_content += padded_counter(total - pending_count + booted);
                line_content += "/";
                line_content += padded_counter(total);
                line_content += "]";
                line_content += ansi::RESET;
                line_content += " ";

                if (info && !info->version.empty()) {
                    line_content += ansi::DIM;
                    line_content += "[";
                    line_content += info->version;
                    line_content += "]";
                    line_content += ansi::RESET;
                    line_content += " ";
                } else {
                    line_content += ansi::YELLOW;
                    line_content += "[!]";
                    line_content += ansi::RESET;
                    line_content += " ";
                }

                std::string pending_line = line_content;
                pending_line += ansi::YELLOW;
                pending_line += "[..]";
                pending_line += ansi::RESET;
                pending_line += " \x1b[38;5;";
                pending_line += std::to_string(mod_color);
                pending_line += "m[";
                pending_line += source;
                pending_line += "]";
                pending_line += ansi::RESET;
                pending_line += " ";
                pending_line += ansi::WHITE;
                pending_line += system->name();
                pending_line += ansi::RESET;
                write_terminal(pending_line);
                flush_terminal();

                // MONOTONIC, not the wall clock: this measures how long one on_start took,
                // and a wall clock can be set - an NTP correction mid-boot would print a
                // negative or absurd duration with nothing to flag it.
                const int64_t start_nanos = ase::utils::monotonic_nanos();
                system->on_start(world.registry());
                const int64_t us =
                    (ase::utils::monotonic_nanos() - start_nanos) / ase::utils::NANOS_PER_MICRO;

                std::string done_line = "\r\x1b[K";
                done_line += line_content;
                done_line += ansi::OK_GREEN;
                done_line += "[OK]";
                done_line += ansi::RESET;
                done_line += " \x1b[38;5;";
                done_line += std::to_string(mod_color);
                done_line += "m[";
                done_line += source;
                done_line += "]";
                done_line += ansi::RESET;
                done_line += " ";
                done_line += ansi::WHITE;
                done_line += system->name();
                done_line += ansi::RESET;

                if (us > BOOT_SLOW_SYSTEM_US) {
                    // "%g" and not a fixed number of decimals: that is the format an
                    // ostream uses for a double, so 1500 us keeps reading "1.5ms" and not
                    // "1.500ms". The conversion is exact, not documented-away.
                    char ms_buffer[MS_BUFFER_BYTES];
                    std::snprintf(ms_buffer, sizeof(ms_buffer), "%g",
                                  static_cast<double>(us) / 1000.0);
                    done_line += ansi::DIM;
                    done_line += " (";
                    done_line += ms_buffer;
                    done_line += "ms)";
                    done_line += ansi::RESET;
                }
                done_line += "\n";
                write_terminal(done_line);
                flush_terminal();

                pending_names.erase(it);
            }
        }
    };

    // Phase 1: L3 Modules (critical infrastructure)
    if (l3_count > 0) {
        boot_layer(false, "L3 Modules", l3_count);
    }

    // Phase 2: L4 Plugins (optional, depends on L3)
    if (l4_count > 0) {
        boot_layer(true, "L4 Plugins", l4_count);
    }

    // Footer
    std::string pending_footer = "\n";
    pending_footer += ansi::DIM;
    pending_footer += line;
    pending_footer += ansi::RESET;
    pending_footer += "\n\n";
    write_terminal(pending_footer);
    flush_terminal();

    // Die gesammelten Zeilen durch die gemerkten Senken abspielen, dann die Klammer schliessen
    // und die Senken zurueckstellen — der Prozess laeuft weiter und braucht sie.
    //
    // Hier filterten zwei dynamic_casts die Konsolen-Senken aus dem Abspielen heraus. Sie sind
    // ersatzlos entfallen, weil sie nichts gefunden haben: im Server-Pfad haengen am Logger ein
    // Datei-Sink, ein HTTP-Ringpuffer und ein Zaehler — GEMESSEN in ase-log, das genau zwei
    // Senken baut (beides Ringpuffer) und in finalize_logger_after_boot genau drei anhaengt,
    // keine davon eine Konsole. Die einzige Konsolen-Senke des Moduls entsteht in
    // init_standalone fuer CLI-Werkzeuge, die keine Boot-Tabelle zeichnen.
    //
    // Der Filter suchte also nach etwas, das hier nie am Logger haengt. Ihn mitzunehmen haette
    // einen wirkungslosen Mechanismus in ein zweites Modul getragen; die Begruendung dafuer
    // steht jetzt an capture_replay() in log.hpp, wo sie neu entschieden werden muss, falls je
    // eine Konsolen-Senke dazukommt.
    (void)log::capture_replay();
    log::capture_end(true);
}

}  // namespace ase::ecs::internal
