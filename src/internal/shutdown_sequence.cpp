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
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-02-01
 * @modified    2026-10-05
 * @version     1.1.0
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
#include <ase/log/log_lifecycle.hpp>  // die Capture-Klammer um die Abschalt-Tabelle
#include <ase/log/log_module.hpp>
#include <ase/log/colors.hpp>

#include <ase/platform/sleep.hpp>
#include <ase/utils/clock.hpp>

#include <cstdio>
#include <string>
#include <ase/containers/hash_map.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

namespace {

// Hier stand eine eigene Queue-Senke, die von der Logbibliothek erbte und den Senkenvektor des
// Loggers von Hand austauschte. boot_logger.cpp trug dieselbe Klasse ein zweites Mal. Beide
// sind seit 2026-08-20 durch die Klammer log::capture_begin/count/entry/end ersetzt: die
// Senken gehoeren dem Logger, also gehoert das Umhaengen in ase-log. Ueber die Schnittstelle
// kommt kein Typ der Logbibliothek mehr — eine kleine Ganzzahl fuer die Stufe, der Text durch
// einen eigenen Puffer.

// Puffer fuer EINE abgespielte Logzeile. Die Klammer gibt die volle Laenge zurueck, auch wenn
// sie mehr ist als hier hineinpasst; die Schleife unten vergleicht beides und haengt eine
// Marke an, statt eine gekuerzte Zeile als vollstaendig auszugeben.
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

    // DIE EINE ZEILE, DIE IN DER LOGDATEI BLEIBT — und sie steht bewusst HIER, vor der
    // Umleitung. Alles ab capture_begin() zeichnet ueber write_terminal, weil die Tabelle
    // ihre Zeilen NEU SCHREIBT und dabei ANSI-Sequenzen und eine feste Spaltenform traegt;
    // durch einen Logsink geleitet waere genau diese Form nicht mehr garantiert (Zeitstempel,
    // Level-Praefix, Zeilenumbrueche des Sinks). DER TERMINAL-TRANSPORT WIRD DESHALB NICHT
    // ANGETASTET — hier kommt nichts weg und nichts wird umgeleitet, es kommt EINE Zeile
    // davor hinzu.
    //
    // WARUM SIE GEBRAUCHT WIRD: ohne sie hinterlaesst ein sauberer Shutdown in der Logdatei
    // KEINE Spur. Ein Tier, der ordentlich heruntergefahren wurde, und einer, der mitten im
    // Tick weggerissen wurde, sehen dort identisch aus — beide enden abrupt in der letzten
    // Arbeitszeile. Am 2026-08-28 liess sich deshalb an keinem der fuenf Tier-Logs
    // feststellen, ob sie sauber gestoppt hatten; die Tabelle, die es beantwortet haette, war
    // mit dem Konsolenfenster verschwunden.
    //
    // INF, nicht DBG: der on_stop-Downstrap je System loggt auf DBG und ist damit im
    // Normalbetrieb (`--log +ERR +WRN +INF`) unsichtbar — gemessen 0 DBG-Zeilen in engine,
    // dist und replica. Eine Marke, die nur bei eingeschaltetem Debug erscheint, beantwortet
    // die Frage nicht. Das Gegenstueck beim Start steht ebenfalls auf INF.
    //
    // KEINE ABSCHLUSSZEILE IM TIER-SERVER, und das ist kein Versaeumnis: dort stellt
    // capture_end(false) die Senken bewusst NICHT zurueck (der Prozess endet direkt danach mit
    // _exit), eine Zeile danach ginge ins Leere. Diese Marke belegt den EINTRITT in den Abbau —
    // dass App::shutdown() erreicht wurde statt eines harten Todes. Fuer den vollstaendigen
    // Verlauf bleibt die Tabelle zustaendig, dort wo sie hingehoert: am Terminal.
    // Ein eingebetteter Host (ShutdownConfig::restore_log_sinks) bekommt seine Senken am Ende
    // zurueck; dort erreicht auch eine Zeile nach dem Abbau die Datei und den Callback.
    log::info("[Shutdown] sequence entered ({} systems)", total);

    // UND SOFORT AUF DIE PLATTE. Ohne diesen Flush existiert die Zeile, erreicht die Datei
    // aber nie: spdlog puffert, die naechste Anweisung ERSETZT mit capture_begin() die Senken,
    // und der Prozess endet mit _exit(0) ohne einen einzigen Destruktor. Der Puffer der alten
    // Senke wird damit von niemandem mehr geleert.
    //
    // GEMESSEN am 2026-08-28: die Marke stand im Binary (strings: 1 Treffer, Bau 11:54:10 neuer
    // als die Quelle), der Kategorie-Filter liess sie durch (das Log trug DBG-Zeilen mehrerer
    // Quellen), die Signalkette bis zum Shutdown-Pfad war intakt (SIGTERM ueber signalfd,
    // nachgestellt mit der Maske eines echten Tiers) — und trotzdem war sie nach zwei
    // DIST-Stopps null Mal in logs/dist-9080.log. Vier gruene Teilpruefungen und ein leeres
    // Ergebnis: der Puffer war die einzige Stelle, an der die Zeile noch verschwinden konnte.
    log::flush();

    // Logausgabe umleiten, damit keine Zeile die Fortschrittstabelle unten zerreisst.
    // Der Rueckgabewert wird nicht geprueft: false heisst "es gibt noch keinen Logger", und
    // dann gibt es auch nichts umzuleiten — die Tabelle laeuft in beiden Faellen gleich.
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
    header += " systems)";
    header += ansi::RESET;
    header += "\n";
    header += ansi::DIM;
    header += line;
    header += ansi::RESET;
    header += "\n";
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
            // not run yet, and the line below replaces exactly this one.
            std::string pending = line_content;
            pending += ansi::YELLOW;
            pending += "[..]";
            pending += ansi::RESET;
            pending += " ";
            pending += line_suffix;
            draw_terminal(config.render_terminal_table, pending);

            // Call on_stop
            (*it)->on_stop(world.registry());

            // Overwrite with [OK]
            std::string done = "\r\x1b[K";
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

    // Footer
    std::string footer = "\n";
    footer += ansi::DIM;
    footer += line;
    footer += ansi::RESET;
    footer += "\n\n";
    draw_terminal(config.render_terminal_table, footer);

    // DER EINGEBETTETE HOST BEKOMMT SEINE SENKEN ZURUECK - und mit ihnen die Zeilen aus on_stop.
    // capture_replay spielt sie durch genau die Senken ab, die bei capture_begin aktiv waren
    // (beim Vivarium-Client: Callback und Datei aus init_tui_standalone), capture_end(true)
    // haengt diese Senken wieder ein. Der Host lebt nach dem Abbau weiter; jede spaetere Zeile -
    // Entladen der Plugins, ein Fehler beim Abbau, der naechste Neustart - braucht sie.
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
            const size_t   idx  = static_cast<size_t>(level);  // schon auf 0..5 begrenzt

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
                // Sichtbar machen statt verschweigen: die Klammer meldet die volle Laenge,
                // also kann eine gekuerzte Zeile hier nicht als vollstaendige durchgehen.
                replay += ansi::DIM;
                replay += " [gekuerzt]";
                replay += ansi::RESET;
            }
            replay += "\n";
            write_terminal(replay);
        }
        // One flush for the whole replay, as before: the loop above deliberately did not
        // flush per line.
        flush_terminal();
    }

    // Die Senken werden im Tier-Server bewusst NICHT zurueckgestellt: der Prozess endet hier,
    // und die Klammer hat ihren Inhalt oben bereits auf das Terminal abgespielt. Genau dafuer
    // nimmt capture_end ein Argument — boot_logger.cpp und der Host-Zweig oben rufen dieselbe
    // Funktion mit true.
    log::capture_end(false);
}

}  // namespace ase::ecs::internal
