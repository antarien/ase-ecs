/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        app.cpp
 * @brief       Application assembly and the tick loop that drives it
 * @description Builds the world from kernel, modules and plugins, then runs
 *              the schedules through TickScheduler. This is where a process
 *              becomes an ASE application: everything above it registers
 *              systems, everything below it only stores or sorts them.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2025-12-01
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

#include <ase/ecs/app.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_mod_tim_comp.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_shtd_comp.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_tim_comp.hpp>
#include <ase/ecs/components/app/tag/ecs_app_quit_req_tag.hpp>
#include <ase/ecs/components/app/tag/ecs_app_rgn_gate_tag.hpp>
// Region-Domain-Gate: die RegionRect-Nahtzeile (L0) ist das Coverage-Signal
#include <ase/types/region_wire.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // log::shutdown beim Herunterfahren der App
#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/tick_scheduler.hpp>

#include <ase/platform/sleep.hpp>
#include <ase/utils/clock.hpp>

#include <csignal>

// Signalzustellung ueber einen Deskriptor statt ueber einen Handler (siehe den Block im anonymen
// Namensraum). <csignal> bleibt fuer sigset_t und die SIG*-Namen; die drei folgenden Header sind
// POSIX und tragen signalfd/read/close/pthread_sigmask.
#include <pthread.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace ase::ecs {

namespace {

/**
 * GRACEFUL SHUTDOWN OHNE SIGNALHANDLER — umgebaut am 2026-08-22.
 *
 * SIGINT/SIGTERM setzen das Laufflag der App auf false, damit App::run() austritt und
 * App::shutdown() das on_stop jedes Systems faehrt. Zentral in App::startup() eingerichtet,
 * damit ALLE Tier-Server (world/reasoning/replica/engine/dist) den Abbau bekommen — das stellt
 * den per-main-Weg wieder her, der im Refactor „streamline main entry point" (world commit
 * 104af81) verlorenging. Seit der Schleifen-Vereinheitlichung rufen alle fuenf mains run();
 * eigene while-Schleifen je Server gibt es nicht mehr.
 *
 * HIER STAND EIN SIGNALHANDLER, und die Regel SIGNAL_HANDLERS_FORBIDDEN verbietet ihn mit der
 * Begruendung „Use ECS event pattern". Das war keine Formalie: ein Handler laeuft im
 * Signalkontext und darf dort fast nichts — kein malloc, keine Sperre, kein Registry-Zugriff.
 * Genau deshalb konnte er nur einen atomaren Store tun und brauchte einen globalen App*-Zeiger,
 * um ueberhaupt an die Instanz zu kommen.
 *
 * DIE AUFLOESUNG IST NICHT „woanders hin": die Regel traegt `file_filter: null` und gilt in
 * JEDER Datei des Baums, auch in den fuenf main.cpp. Ein Umzug nach L5 haette aus einer
 * Fundstelle fuenf gemacht und nichts geloest. Ein ersatzloser Wegfall haette fuenf Servern den
 * geordneten Shutdown genommen.
 *
 * STATTDESSEN VERSCHWINDET DIE SACHE SELBST: die Signale werden fuer den Prozess BLOCKIERT und
 * ueber einen signalfd zugestellt. Damit gibt es keinen asynchronen Rueckruf mehr, keinen
 * Signalkontext, keine async-signal-safety-Beschraenkung und keinen globalen App*-Zeiger. Das
 * Signal ist ein EREIGNIS, das der Tick abholt — woertlich das, was die Regelmeldung verlangt.
 *
 * DER PREIS, ehrlich benannt: das Flag faellt nicht mehr sofort, sondern beim naechsten Tick
 * (≤ 16,7 ms bei 60 Hz). Beim Herunterfahren belanglos. Und bei einem HAENGENDEN System waren
 * beide Formen ohnehin gleich hilflos — der alte Handler setzte das Flag sofort, aber die
 * Schleife waere nie zur Abfrage zurueckgekehrt.
 *
 * DER DESKRIPTOR LIEGT IN EINEM COMPONENT, nicht in einer Dateivariablen. Hier stand zuerst ein
 * `int g_shutdown_fd` — und die zweite Regel, GLOBAL_VARIABLE_FORBIDDEN, haette ihn getroffen
 * („Use Components or registry.ctx()"). Der alte `std::atomic<App*> g_signal_app` entging ihrem
 * Muster nur, weil `std::atomic<...>` kein `\w+\s+g_\w+` ist; die Sache war dieselbe.
 *
 * BEIDE REGELN ZEIGEN AUF DIESELBE BAUFORM, und das ist kein Zufall: ohne Signalkontext darf
 * der Zustand dort liegen, wo ECS-Zustand liegt. Siehe EcsAppStaShtdComponent — die ANWESENHEIT
 * des Components heisst „Wache scharf", weshalb der Deskriptor keinen Ungueltigkeitswert
 * braucht.
 */

/**
 * Richtet die Zustellung ein und liefert den Deskriptor, oder einen negativen Wert.
 *
 * pthread_sigmask BLOCKIERT die Signale prozessweit — ohne das wuerde SIGINT weiterhin das
 * Vorgabeverhalten ausloesen (sofortiger Tod) und der signalfd niemals etwas sehen. SIGHUP ist
 * mit in der Maske: ein Terminal-Hangup darf einen kopflosen Tier-Server nicht toeten, und eine
 * blockierte Zustellung tut genau das, was SIG_IGN vorher tat.
 *
 * SIGPIPE STEHT AUS DEMSELBEN GRUND DABEI, ergaenzt am 2026-08-28. Der Tier laeuft in einer
 * Pipeline, deren Leser der ase-logview ist (core/ase-console — er legt die Pipe an, startet
 * den Tier als sein Kind und liest dessen stdout), und die Bootphase
 * schreibt ihre Fortschrittstabelle auf stdout. Verlaesst der Betrachter das Fenster, waehrend
 * der Server noch schreibt, toetet ihn der naechste Schreibvorgang sofort und lautlos — kein
 * Coredump, kein Journal, keine Shutdown-Zeile (logview_main.cpp:951 beschreibt genau diesen
 * Tod). Blockiert kann er nicht toeten; der Schreibvorgang scheitert dann mit EPIPE, was der
 * richtige Weg ist.
 *
 * WARUM DAS EINE AUSNAHME BESEITIGT UND KEINE SCHAFFT: gemessen am 2026-08-28 trugen dist und
 * engine SIGPIPE als ignoriert (SigIgn Bit 13), replica und der Edge-Daemon NICHT — und im
 * ganzen Baum setzt es keine einzige Zeile. Der Unterschied kam also aus einer Bibliothek, die
 * der eine Tier laedt und der andere nicht: eine Eigenschaft per Zufall der Ladeliste. Hier
 * gesetzt, gilt sie fuer alle fuenf Tiers gleich und an einer Stelle.
 *
 * DIESE ZEILE WIRD NICHT ENTFERNT. Am 2026-08-28 habe ich sie einmal wieder herausgenommen,
 * weil ich SIGPIPE fuer den Beendigungsweg der Konsole hielt — das war falsch und der
 * Betreiber hat es als Sabotage zurueckgewiesen. Der Verlust der Doppel-Strg+C-Sicherung kam
 * NICHT von hier. Wer den Ausfall dieser Sicherung sucht, sucht in der KONSOLE
 * (core/ase-console): sie haelt den ersten Druck, sie schickt beim zweiten das SIGTERM, und sie
 * haelt den Tier in einer eigenen Prozessgruppe, damit ihn kein Tastendruck direkt trifft.
 * In dieser Maske steht darueber nichts.
 */
int open_shutdown_watch() {
    // Der Aufrufwert -1 heisst „neuen Deskriptor anlegen"; derselbe Wert kommt im Fehlerfall
    // zurueck. Beides ist POSIX und steht hier lokal, nicht auf Dateiebene.
    constexpr int NewDescriptor = -1;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGPIPE);

    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
        return NewDescriptor;
    }
    return ::signalfd(NewDescriptor, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
}

/**
 * Holt ein anstehendes Abbruchsignal ab. true heisst „der Betreiber will beenden".
 *
 * Nichtblockierend: liegt nichts an, liefert ::read -1 mit EAGAIN und der Tick laeuft weiter.
 * SIGHUP, SIGPIPE und SIGINT werden gelesen und VERWORFEN — sie sind blockiert, damit sie nicht
 * toeten, nicht damit sie beenden. Ohne dieses Auslesen blieben sie im Deskriptor stehen.
 *
 * SIGINT ZAEHLT WIEDER ALS BEENDIGUNGSWUNSCH — UND DAS IST DIE RUECKNAHME EINER FALSCHEN
 * ADRESSE. Zwischen dem 2026-08-28 und dem 2026-09-06 stand SIGINT in dieser Verwerfungsliste,
 * mit einer Begruendung, die aus einem BETRACHTER stammte: der ase-logview faengt den ersten
 * Strg+C ab, zeigt den roten Footer, und erst der zweite sollte beenden. Die Doppelbestaetigung
 * ist richtig — ihr Ort war es nicht. Ein Tier laeuft auf einer eigenen VM, unter systemd, ohne
 * Tastatur und ohne Konsole; ihm eine Terminal-Geste einzubauen, macht ihn genau dort
 * unbeendbar, wo niemand sie ausloesen kann: eine SSH- oder Mosh-Sitzung, in der ein Betreiber
 * das Binaer im Vordergrund startet, reagierte auf Strg+C ueberhaupt nicht mehr.
 *
 * WO DIE GESTE JETZT LIEGT: in der Konsole, die sie anzeigt. Der ase-logview startet den Tier
 * als eigenes Kind in einer EIGENEN PROZESSGRUPPE (core/ase-console, spawn_in_group) — das
 * Terminal stellt Strg+C nur der Vordergrundgruppe zu, der Tier bekommt also gar keines, und
 * beim zweiten Druck schickt die Konsole ihm SIGTERM. Die Sicherung haengt damit an einer
 * Prozessgruppe statt an einer Sonderregel in einem kopflosen Dienst, und sie wird mit dem
 * Konsolenbinaer ausgeliefert statt in einem Skript zu stehen, das nie eine VM erreicht.
 *
 * VERWORFEN BLEIBEN SIGHUP UND SIGPIPE, und die beiden aus verschiedenen Gruenden: ein
 * Terminal-Hangup darf einen kopflosen Tier nicht toeten, und ein geschlossener Betrachter ist
 * kein Abbruchwunsch — sein Schreibfehler heisst EPIPE und wird dort behandelt, wo geschrieben
 * wird. Aus der MASKE herauszunehmen hiesse toeten lassen; aus dieser LISTE herauszunehmen
 * hiesse beenden auf ein Signal, das kein Beenden meint.
 */
bool shutdown_requested(int fd) {
    bool requested = false;
    struct signalfd_siginfo info {};
    while (::read(fd, &info, sizeof(info)) == static_cast<long>(sizeof(info))) {
        const bool is_quit_request = info.ssi_signo != static_cast<uint32_t>(SIGHUP) &&
                                     info.ssi_signo != static_cast<uint32_t>(SIGPIPE);
        if (is_quit_request) {
            requested = true;
        }
    }
    return requested;
}

/**
 * Gibt die Zustellung zurueck. Nach dem Aufheben der Maske gilt wieder das Vorgabeverhalten —
 * ein zweites Strg+C waehrend des Abbaus beendet hart, statt in eine halb abgebaute App zu
 * laufen. Das ist dieselbe Absicht, die vorher das Zuruecksetzen auf SIG_DFL hatte.
 *
 * DIESER SATZ GALT NEUN TAGE LANG NICHT — vom 2026-08-28 bis zum 2026-09-06 — und weggefallen
 * ist die Bedingung, nicht der Satz. Das Aufheben einer Maske stellt die VORGABE
 * nur her, wenn die Disposition die Vorgabe IST; damals startete ein Shell-Skript den Server
 * als Hintergrundjob, und POSIX schreibt dafuer in einer Shell ohne Job-Control SIG_IGN fuer
 * SIGINT/SIGQUIT vor — eine Disposition, die exec ueberlebt (gemessen: SigIgn=0x1006 gegen
 * 0x1000 im Vordergrund). Der Notausgang fiel damit nicht auf „hart beenden", sondern auf
 * „wirkungslos".
 *
 * HEUTE STARTET DIE KONSOLE DEN TIER SELBST, per fork/exec ohne Zwischenshell
 * (core/ase-console, spawn_in_group). Damit erbt er keine Ignorierung mehr: ein Handler wird
 * ueber exec ohnehin auf Vorgabe zurueckgesetzt, und SIG_IGN setzt niemand. Der Satz oben gilt
 * also wieder — und er ist ueberdies nicht mehr die Sicherung: dass ein einzelner Tastendruck
 * den Tier nicht beendet, entscheidet die Prozessgruppe der Konsole, nicht seine Signalmaske.
 *
 * ZWISCHEN Armierung und Abbau war auch damals nichts kaputt: ein blockiertes Signal wird
 * pending, nie verworfen — SIG_IGN gewinnt dort NICHT (gegengeprueft mit einem
 * signalfd-Testprogramm). Es waren ausschliesslich die beiden Raender, und der eine davon (das
 * Bootfenster) ist seither durch das fruehe Blockieren geschlossen.
 */
void close_shutdown_watch(int fd) {
    // SIGPIPE FEHLT HIER ABSICHTLICH UND BLEIBT BLOCKIERT. Die drei anderen werden freigegeben,
    // damit ein zweites Strg+C den Abbau hart abkuerzen kann — SIGPIPE meint kein Beenden,
    // sondern trifft den Server genau dann, wenn der Betrachter das Fenster waehrend des
    // Herunterfahrens verlaesst. Freigegeben wuerde er den Tier toeten, BEVOR seine
    // Shutdown-Zeilen in der Logdatei stehen; das ist der Tod ohne Spur aus
    // logview_main.cpp:990. Der Prozess endet unmittelbar danach, eine gesetzte Blockade kostet
    // also nichts.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
    ::close(fd);
}

}  // anonymous namespace

// =============================================================================
// APP IMPLEMENTATION (Orchestrator)
// =============================================================================

App::App()
    : tick_scheduler_(std::make_unique<internal::TickScheduler>())
    , system_registry_(std::make_unique<internal::SystemRegistry>()) {
}

App::~App() = default;

void App::finalize_system(Schedule schedule, std::unique_ptr<System> system,
                          ase::containers::Vector<std::string> after, int priority,
                          std::string source, std::string version) {
    std::string name = system->name();
    std::string src = source.empty() ? current_source_ : std::move(source);
    std::string ver = version.empty() ? current_version_ : std::move(version);
    system_registry_->add_system(schedule, std::move(system), std::move(name), std::move(src),
                          std::move(ver), std::move(after), priority);
}

App& App::declare_module_plane(std::string_view module, uint8_t plane) {
    system_registry_->set_module_plane(std::string(module), plane);
    return *this;
}

void App::startup() {
    /**
     * BLOCKIEREN ZUERST, VERARBEITEN SPAETER — und die Trennung dieser beiden Schritte ist der
     * ganze Punkt. Der Deskriptor entsteht HIER, vor dem Boot; gelesen wird er erst, wenn unten
     * das Component steht. Ein blockiertes Signal geht nicht verloren, es bleibt pending: ein
     * Strg+C waehrend des Boots erreicht die halb gebaute App nicht (die Absicht des frueheren
     * "armed LAST"), wird aber beim ersten Tick nach dem Boot abgeholt und faehrt den Tier
     * sauber herunter.
     *
     * WARUM DIE SPAETE ARMIERUNG EIN UNBEENDBARER SERVER WAR — gemessen am 2026-08-28 am echten
     * dist-Binary durch den echten Startpfad: 30 Sekunden lang stand SigBlk auf 0x10000 (nur
     * SIGCHLD), die Wache war nie armiert, SigIgn trug 0x6. In diesem Fenster war SIGINT nicht
     * etwa Vorgabe, sondern IGNORIERT — der damalige Startpfad startete den Server als
     * Hintergrundjob einer Shell ohne Job-Control, und POSIX setzt dafuer SIG_IGN. Ein SIGINT an
     * die Prozessgruppe liess den Server unbeeindruckt weiterlaufen.
     *
     * Diese Erbschaft gibt es seit dem 2026-09-06 nicht mehr (die Konsole forkt den Tier selbst),
     * aber der Grund fuer das fruehe Blockieren bleibt unveraendert: ein Signal, das ankommt,
     * bevor die Wache liest, waere sonst verloren — gleich, welche Disposition davor stand.
     *
     * Und das Fenster ist nicht kurz: es umfasst den GESAMTEN Boot samt aller Wiederholungen.
     * Haengt eine Verbindung im Retry, ist der Tier ueberhaupt nicht mehr per Strg+C zu
     * beenden — genau der Befund, mit dem dieser Tag begann.
     */
    const int watch_fd = open_shutdown_watch();

    // Sort systems by dependencies (FIXED: no null pointer bug)
    auto errors = internal::sort_systems_by_dependencies(*system_registry_);
    if (!errors.empty()) {
        // Log cycle errors but continue (systems run in original order)
        for (const auto& err : errors) {
            // Error already logged by dependency_sorter
            (void)err;
        }
    }

    // Mark boot started — systems added after this point are queued as pending
    system_registry_->mark_boot_started();

    // Print boot log and call on_start for each system
    internal::BootLoggerConfig boot_config;
    boot_config.boot_delay_us = boot_delay_us_;
    internal::print_boot_sequence(*system_registry_, world_, boot_config);

    // Late-System-Registration: if on_start() added new systems (e.g., dlopen modules),
    // re-sort and boot the pending systems. Their on_start() runs in boot_pending_systems.
    if (system_registry_->has_pending()) {
        internal::sort_systems_by_dependencies(*system_registry_);
        internal::boot_pending_systems(*system_registry_, world_, boot_config);
    }

    // Run Lifecycle schedules (Initialization → Configuration) once at startup.
    // Restores the pre-e75a4fd behaviour: systems registered in Initialization
    // or Configuration need their tick() invoked once so effects like MongoDB
    // store_pool() actually fire. Commit a1b396d established the ordering —
    // Initialization before Configuration.
    run_schedule(Schedule::Initialization, 0.0f);
    run_schedule(Schedule::Configuration, 0.0f);

    running_.store(true);
    last_frame_time_ = utils::monotonic_nanos();

    /**
     * Die Wache SCHARFSTELLEN, jetzt da der Boot durch ist: Ctrl+C (SIGINT) oder ein kill
     * (SIGTERM) brechen die Laufschleife, damit shutdown() den on_stop-Abbau faehrt. SIGHUP
     * wird geschluckt (ein Terminal-Hangup darf einen kopflosen Tier-Server nicht toeten).
     *
     * BLOCKIERT WURDE OBEN, GELESEN WIRD AB HIER — die beiden Schritte sind absichtlich
     * getrennt. Das Component ist der Schalter: erst seine Anwesenheit laesst App::tick() den
     * Deskriptor abfragen. Ein Signal aus der Bootphase liegt bis dahin pending und wird beim
     * ersten Tick abgeholt; es erreicht also keine halb gebaute App und geht trotzdem nicht
     * verloren. Frueher entstand der Deskriptor ERST hier, und damit war der ganze Boot ein
     * Fenster, in dem Strg+C wirkungslos war (SigIgn aus dem Hintergrundstart) — bei einem im
     * Retry haengenden Boot war der Tier ueberhaupt nicht mehr zu beenden.
     *
     * SCHEITERT DAS SCHARFSCHALTEN, ENTSTEHT KEIN COMPONENT — und dann meldet es sich, statt
     * still ein Nichts zu sein. Ohne Wache laeuft der Server weiter, aber Strg+C beendet ihn
     * hart: on_stop faellt aus, offene Verbindungen und Puffer werden nicht abgebaut. Das ist
     * genau die Sorte Fehlschlag, die von „Erfolg ohne Arbeit" nicht zu unterscheiden waere.
     */
    if (watch_fd < 0) {
        log::warn(log::WRN::CAT::HOST_OP_FAILED, "App", "shutdown_watch_arm",
                  "SIGINT/SIGTERM will terminate hard, on_stop will NOT run");
        return;
    }

    auto& registry = world_.registry();
    const Entity watch_entity = registry.create();
    registry.emplace<EcsAppStaShtdComponent>(watch_entity).watch_fd =
        static_cast<int32_t>(watch_fd);
}

void App::shutdown() {
    /**
     * Disarm the shutdown watch before teardown so a second Ctrl+C during shutdown reverts to
     * the default disposition (hard exit) instead of stalling on a tearing-down App. Das
     * Aufheben der Maske stellt genau das her, was frueher das Zuruecksetzen auf SIG_DFL tat.
     *
     * Erst SAMMELN, dann loeschen: waehrend der Iteration eines Views darf keine Entity
     * zerstoert werden. Es ist genau eine — der Vektor ist die FORM, nicht die Menge.
     */
    auto& registry = world_.registry();
    ase::containers::Vector<Entity> watch_entities;
    auto watch_view = registry.view<EcsAppStaShtdComponent>();
    for (auto entity : watch_view) {
        close_shutdown_watch(static_cast<int>(watch_view.get<EcsAppStaShtdComponent>(entity).watch_fd));
        watch_entities.push_back(entity);
    }
    for (uint32_t i = 0; i < watch_entities.size(); ++i) {
        registry.destroy(watch_entities[i]);
    }

    // Invoke destroy callback (port of setOnDestroyCallback)
    if (on_destroy_callback_) {
        on_destroy_callback_();
        on_destroy_callback_ = nullptr;
    }

    // Run Finalization schedule
    run_schedule(Schedule::Finalization, 0.0f);

    // Print shutdown sequence and call on_stop for each system
    internal::ShutdownConfig shutdown_config;
    shutdown_config.shutdown_delay_us = shutdown_delay_us_;
    internal::print_shutdown_sequence(*system_registry_, world_, shutdown_config);

    running_.store(false);
}

// =============================================================================
// Introspection API (port of systemRegistry.ts)
// =============================================================================

size_t App::system_count() const {
    return system_registry_->total_count();
}

const ase::containers::Vector<internal::SystemInfo>& App::system_infos() const {
    return system_registry_->infos();
}

const ase::containers::Vector<std::unique_ptr<System>>& App::systems_for(Schedule schedule) const {
    return system_registry_->systems_for(schedule);
}

void App::schedule_trampoline(void* user, Schedule schedule, float sched_dt) {
    static_cast<App*>(user)->run_schedule_measured(schedule, sched_dt);
}

void App::run_schedule_measured(Schedule schedule, float sched_dt) {
    if (schedule == Schedule::Dynamics) {
        // Only the orchestrator brackets a schedule execution: the measured
        // Dynamics wall time feeds the kernel stats EMA and the region-load
        // attribution chain via the EcsAppStaTimComponent singleton.
        const int64_t sim_begin = utils::monotonic_nanos();
        run_schedule(schedule, sched_dt);
        const float sim_ms = static_cast<float>(utils::monotonic_nanos() - sim_begin) /
                             static_cast<float>(utils::NANOS_PER_MILLI);

        auto& registry = world_.registry();
        auto timing_view = registry.view<EcsAppStaTimComponent>();
        const Entity timing_entity =
            (timing_view.begin() != timing_view.end()) ? *timing_view.begin()
                                                       : registry.create();
        auto& timing = registry.get_or_emplace<EcsAppStaTimComponent>(timing_entity);
        timing.dynamics_time_ms = sim_ms;
        timing.dynamics_runs++;
        return;
    }
    run_schedule(schedule, sched_dt);
}

void App::tick(float dt) {
    /**
     * DIE SIGNALABHOLUNG STEHT IN tick(), NICHT IN run(). Historisch, weil drei Server eigene
     * while-Schleifen um tick() fuhren und ein Abholen in run() sie still ohne geordneten
     * Shutdown gelassen haette; diese Schleifen sind in run() aufgegangen und alle fuenf mains
     * rufen run(). Die Platzierung bleibt trotzdem hier: tick() ist der eine Punkt, durch den
     * JEDER Treiber der App kommt — auch ein Test oder ein Werkzeug, das tick() direkt ruft —
     * und die Quit-Tag-Abholung darunter braucht die Registry ohnehin je Tick.
     *
     * Der View ist leer, wenn die Wache nicht scharf ist; dann kostet das hier nichts.
     */
    auto& registry = world_.registry();
    auto watch_view = registry.view<EcsAppStaShtdComponent>();
    for (auto entity : watch_view) {
        if (shutdown_requested(static_cast<int>(watch_view.get<EcsAppStaShtdComponent>(entity).watch_fd))) {
            running_.store(false);
        }
    }

    /**
     * DIE ZWEITE STOPPQUELLE NEBEN DEM SIGNAL: das Quit-Tag. Ein System bekommt die Registry
     * und nie die App — beenden kann den Prozess deshalb nur ein EREIGNIS, das der Tick abholt.
     * Wer oberhalb von L1 fertig ist (heute: KernelCoreLfcSystem im selben Durchlauf, der
     * KernelCoreRunnTag entfernt), stampft EcsAppQuitReqTag; hier wird es gelesen. Vorher trug
     * NUR ase-server-reasoning diese Bruecke, als Sonderschritt in seiner eigenen main-Schleife —
     * die vier anderen Tiers tickten einen gestoppten Kernel endlos weiter. Der View ist leer,
     * solange niemand stoppt; dann kostet das hier nichts.
     */
    auto quit_view = registry.view<EcsAppQuitReqTag>();
    if (quit_view.begin() != quit_view.end()) {
        running_.store(false);
    }

    // Hier stand ein Lambda mit [this]-Capture. Es konnte nicht bleiben, weil der Rueckruf
    // jetzt ein blosser Funktionszeiger ist — ein Lambda mit Capture zerfaellt nicht zu einem
    // solchen. Der Zustand, den die Capture trug, geht denselben Weg wie ueberall sonst im
    // Baum: als undurchsichtiger Zeiger durch den Rueckruf hindurch und am anderen Ende
    // zurueckgeholt.
    tick_scheduler_->tick(dt, &App::schedule_trampoline, this);
}

void App::run() {
    startup();

    /**
     * GETAKTET AUF DIE FRAME-BAND-RATE, NICHT FREILAUFEND — UND DIES IST DIE EINE SCHLEIFE
     * ALLER FUENF TIERS. Der Frame-Tier hat im Ticker interval = 0.0f und laeuft in JEDER
     * Iteration — die Schleifenrate IST die Frame-Band-Rate. Hier stand ein festes
     * 1-ms-Abgeben: damit lief das Frame-Band so schnell, wie die eigene Arbeit es zuliess
     * (Obergrenze 1000 Hz statt der in schedule.hpp deklarierten 60), und ein World-/
     * Replica-Knoten verbrannte gemessen rund 0.8 Kerne, im Leerlauf genauso wie unter Last.
     * Engine/Reasoning/Dist trugen daneben je eine EIGENE Handschleife mit relativem Pacing
     * und einem KONSTANTEN dt von 1/60 — zwei Bauformen, jede mit ihrem eigenen Zeitdefekt
     * (Audit 02-schedule-landkarte.md, Walls 6 und 9). Beide Formen sind hierin aufgegangen.
     *
     * schedule_hz(Schedule::Reception) ist die SSOT der Rate: die 60.0f aus schedule.hpp
     * treibt den Takt, statt unbenutzte Doku zu sein. dt bleibt die GEMESSENE Spanne, nie
     * eine Konstante.
     *
     * ABSOLUTE DEADLINES, NICHT RELATIVER REST: ein Pacer, der `Budget - Arbeit` nachschlaeft,
     * erbt die Aufwachlatenz des Schedulers als frischen Fehler in JEDEM Frame und driftet
     * dauerhaft unter die Nennrate. sleep_until_nanos uebergibt dem Kernel die Deadline
     * selbst; das Verschlafen eines Frames verkuerzt den Schlaf des naechsten um exakt
     * denselben Betrag — die Langzeitrate IST die Nennrate. Ueberzieht ein Tick sein Budget,
     * wird die Deadline NEU VERANKERT statt aufgeholt: eine Aufholjagd wuerde nach einem
     * Stall mehrere Frames ohne Schlaf hintereinander feuern, und die Tier-Akkumulatoren
     * arbeiten ohnehin mit dem gemessenen dt — verlorene Zeit ist dort schon verbucht.
     */
    constexpr int64_t FrameIntervalNanos = static_cast<int64_t>(
        static_cast<float>(utils::NANOS_PER_SECOND) / schedule_hz(Schedule::Reception));

    int64_t next_deadline = utils::monotonic_nanos() + FrameIntervalNanos;

    while (running_.load()) {
        const int64_t now = utils::monotonic_nanos();
        const float frame_dt = static_cast<float>(now - last_frame_time_) /
                               static_cast<float>(utils::NANOS_PER_SECOND);
        last_frame_time_ = now;

        tick(frame_dt);

        const int64_t after = utils::monotonic_nanos();
        if (after < next_deadline) {
            ase::platform::sleep_until_nanos(static_cast<uint64_t>(next_deadline));
            next_deadline += FrameIntervalNanos;
        } else {
            next_deadline = after + FrameIntervalNanos;
        }
    }

    shutdown();

    /**
     * HIER ENDET DER PROZESS, UND ZWAR VOR DEN STACK-DESTRUKTOREN. `ARCH_ASE_REP_SRV.md`
     * (Abschnitt „libdatachannel: _exit(0) nach ECS-Shutdown") schreibt das fuer jeden Server
     * mit libdatachannel-WebSockets vor: die rtc-Objekte blockieren beim Abbau in JEDER
     * Variante — ws_->close(), forceClose(), reset() ohne close, connections_.clear() —, weil
     * ihr Destruktor auf Close-Handshake und Thread-Join wartet. Ein `return 0` laeuft genau
     * dort hinein und haengt; das OS raeumt die TCP-Sockets ohnehin auf.
     *
     * WARUM DIE ZEILE HIER STEHT UND NICHT FUENFMAL IN DEN main.cpp: der Vorschrift fehlte seit
     * dem 2026-04-12 ihr Ort. Der Refactor „streamline main entry point" zog http/http_thread
     * aus den Tier-mains in die L4-Webserver-Plugins und nahm den ganzen Shutdown-Block mitsamt
     * `_exit(0)` mit; die Doku beschreibt seither eine main(), die es nicht mehr gibt, und alle
     * fuenf Tiers standen auf `return 0`. Gemessen am 2026-08-28: App::run() hat GENAU FUENF
     * Aufrufer, die fuenf Tier-mains, und jede tut danach nur noch `return 0;` — kein Test, kein
     * Werkzeug, kein Client ruft es. Damit ist dies der einzige Ort, an dem die Regel fuer alle
     * gilt, ohne sie fuenfmal zu wiederholen.
     *
     * DER LOGGER WIRD VORHER GESCHLOSSEN, weil _exit auch seine Puffer ueberspringt. Ohne das
     * waeren die Shutdown-Zeilen genau in dem Lauf verloren, dessen Ende sie belegen sollen —
     * dieselbe Signatur (Log endet mitten im Tick, keine Stopp-Zeile), an der ein abgewuergter
     * Tier von einem sauber beendeten nicht zu unterscheiden ist.
     *
     * ctx() BLEIBT DABEI UNGERAEUMT, und das ist Absicht statt Versaeumnis: App::shutdown()
     * faehrt on_stop jedes Systems (KernelWbskConnSystem::on_stop ruft clear_all()), laesst den
     * WebSocketResourceManager aber im ctx() stehen. Sein Destruktor ist genau der blockierende,
     * den diese Zeile ueberspringt.
     */
    ase::log::shutdown();
    ::_exit(0);
}

void App::run_schedule(Schedule schedule, float dt) {
    auto& systems = system_registry_->systems_for(schedule);
    if (systems.empty()) {
        return;
    }

    /**
     * M-B module axis (PLAN_ASE_COMPUTE_MOD_AXIS.md T3): bracket every system
     * tick and roll the wall time up per source module. The info indices run
     * in lockstep with the systems vector (reorder_schedule permutes both), so
     * the registration-time (mod_hash, grp_id) attribution is O(1) per system.
     */
    const auto& idx_map = system_registry_->infos_by_schedule();
    auto idx_it = idx_map.find(schedule);
    const auto& infos = system_registry_->infos();

    /**
     * REGION-DOMAIN-GATE (Betreiber-Festlegung 2026-08-26): Ein Knoten ohne eigene Region
     * simuliert NICHTS — Vorgabe jedes Moduls und Plugins ist die SIMULATION-Ebene, und die
     * wird ohne Coverage gehalten; nur die im Manifest erklaerte Infrastruktur
     * (APP_PLANE_KERN) tickt immer. Das Tor sitzt HIER und nur hier: je System entscheidet
     * die bei der Registrierung gestempelte Ebene (SystemInfo.plane, deklariert aus
     * module.toml `plane` VOR dem Laden) — kein einziger Systemrumpf traegt eine eigene
     * Pruefung, neue Module erben die Vorgabe ohne jede Eintragung.
     *
     * SCHARF ist das Tor nur, wo der Kernel des Prozesses den EcsAppRgnGateTag-Anker
     * gestempelt hat (Tier::World — die Compute-Schicht); auf jedem anderen Tier ist die
     * Schleife byte-identisch zum Stand ohne Tor. Coverage ist die RegionRect-Nahtzeile
     * (L0-POD) des Zuweisungsempfaengers — die RAUM-Achse und nur sie: Projekt- oder
     * Tenant-Filter waeren das verbotene Silo-Modell (ARCH_ASE_TOPOLOGY.md).
     *
     * Lifecycle-Schedules (Initialization/Configuration/Termination/Finalization) laufen
     * auch fuer gehaltene Module: Manager-Geburt und Abbau bleiben unversehrt, gehalten
     * wird nur die laufende Arbeit.
     */
    bool region_hold = false;
    if (world_.registry().storage<EcsAppRgnGateTag>().size() > 0u) {
        const size_t rect_rows = world_.registry().storage<types::RegionRect>().size();
        const int hold_state = (rect_rows == 0u) ? 1 : 0;
        if (hold_state != region_hold_state_) {
            region_hold_state_ = hold_state;
            if (hold_state == 1) {
                log::info("[App] no owned RegionRect rows - SIMULATION plane held "
                          "(lifecycle schedules excepted, KERN infrastructure keeps running)");
            } else {
                log::info("[App] region coverage present ({} RegionRect row(s)) - "
                          "SIMULATION plane running",
                          rect_rows);
            }
        }
        region_hold = (hold_state == 1) && !is_lifecycle_schedule(schedule);
    }

    ase::containers::Vector<uint32_t> pass_hash;
    ase::containers::Vector<uint32_t> pass_grp;
    ase::containers::Vector<uint64_t> pass_us;

    for (size_t i = 0; i < systems.size(); ++i) {
        auto& system = systems[i];
        if (!system || !system->enabled()) {
            continue;
        }
        const internal::SystemInfo* gate_info = nullptr;
        if (idx_it != idx_map.end() && i < idx_it->second.size()) {
            gate_info = &infos[idx_it->second[i]];
        }
        if (region_hold && gate_info != nullptr && gate_info->plane == APP_PLANE_SIM) {
            continue;  // held plane: no coverage, no work - the gate, not the system, decides
        }
        const int64_t sys_begin = utils::monotonic_nanos();
        system->tick(world_.registry(), dt);
        const uint64_t sys_us = static_cast<uint64_t>(
            (utils::monotonic_nanos() - sys_begin) / utils::NANOS_PER_MICRO);

        if (gate_info == nullptr) {
            continue;  // lockstep info missing (never expected) - keep ticking, skip attribution
        }
        const internal::SystemInfo& info = *gate_info;
        bool merged = false;
        for (size_t p = 0; p < pass_hash.size(); ++p) {
            if (pass_hash[p] == info.mod_hash) {
                pass_us[p] += sys_us;
                merged = true;
                break;
            }
        }
        if (!merged) {
            pass_hash.push_back(info.mod_hash);
            pass_grp.push_back(info.grp_id);
            pass_us.push_back(sys_us);
        }
    }

    /**
     * Upsert one EcsAppStaModTimComponent row per touched (module, schedule).
     * Running totals only ever grow (dlt_count/dlt_seen cursor discipline);
     * consumers difference against their own cursors.
     */
    auto& registry = world_.registry();
    const uint32_t sched_id = static_cast<uint32_t>(schedule);
    for (size_t p = 0; p < pass_hash.size(); ++p) {
        const uint64_t key = (static_cast<uint64_t>(pass_hash[p]) << 32) | sched_id;
        auto row_it = mod_tim_rows_.find(key);
        if (row_it == mod_tim_rows_.end() || !registry.valid(row_it->second)) {
            mod_tim_rows_[key] = registry.create();
            row_it = mod_tim_rows_.find(key);
        }
        auto& tim = registry.get_or_emplace<EcsAppStaModTimComponent>(row_it->second);
        tim.mod_hash = pass_hash[p];
        tim.grp_id = pass_grp[p];
        tim.sched_id = sched_id;
        tim.time_us += pass_us[p];
        tim.runs++;
    }
}

}  // namespace ase::ecs
