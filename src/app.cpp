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
 * @design      DSGN_016
 * @category    ecs/module
 * @usecase     usecase:100357 - run the tick loop
 * @actor       Entity Component System (ECS)
 * @created     2025-12-01
 * @modified    2026-10-25
 * @version     1.4.0
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
#include <ase/ecs/components/app/request/ecs_app_req_schd_comp.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_mod_tim_comp.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_shtd_comp.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_tim_comp.hpp>
#include <ase/ecs/components/app/tag/ecs_app_quit_req_tag.hpp>
#include <ase/ecs/components/app/tag/ecs_app_rgn_gate_tag.hpp>
// Region domain gate: the RegionRect seam row (L0) is the coverage signal
#include <ase/types/region_wire.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // log::shutdown when the App goes down
#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/tick_scheduler.hpp>

#include <ase/platform/sleep.hpp>
#include <ase/utils/clock.hpp>

#include <csignal>

// Signal delivery through a descriptor instead of a handler (see the block in the anonymous
// namespace). <csignal> stays for sigset_t and the SIG* names; the three headers below are POSIX
// and carry signalfd/read/close/pthread_sigmask.
#include <pthread.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace ase::ecs {

namespace {

/**
 * GRACEFUL SHUTDOWN WITHOUT A SIGNAL HANDLER
 *
 * SIGINT/SIGTERM set the run flag of the App to false, so App::run() leaves its loop and
 * App::shutdown() runs the on_stop of every system. It is set up centrally in App::startup(), so
 * that ALL tier servers (world/reasoning/replica/engine/dist) get the teardown; all five mains
 * call run(), none runs a loop of its own.
 *
 * NO HANDLER, because the rule SIGNAL_HANDLERS_FORBIDDEN forbids it with the reason "Use ECS event
 * pattern", and that is no formality: a handler runs in signal context and may do almost nothing
 * there - no malloc, no lock, no registry access. It could only do an atomic store and would need
 * a global App* pointer to reach the instance at all. The rule carries `file_filter: null` and
 * holds in EVERY file of the tree, the five main.cpp included, so moving the handler elsewhere
 * would solve nothing.
 *
 * THE SIGNALS ARE BLOCKED for the process and delivered through a signalfd. With that there is no
 * asynchronous callback, no signal context, no async-signal-safety restriction and no global App*
 * pointer. The signal is an EVENT the tick picks up - literally what the rule message asks for.
 *
 * THE PRICE, named honestly: the flag falls at the next tick, not at once (at most 16.7 ms at
 * 60 Hz). Irrelevant when shutting down. With a HANGING system a handler would be equally
 * helpless: it would set the flag at once, but the loop would never come back to the query.
 *
 * THE DESCRIPTOR LIVES IN A COMPONENT, not in a file variable: the rule GLOBAL_VARIABLE_FORBIDDEN
 * ("Use Components or registry.ctx()") points to the same form as the first one. Without signal
 * context the state may live where ECS state lives. See EcsAppStaShtdComponent - the PRESENCE of
 * the component means "watch armed", which is why the descriptor needs no invalid value.
 */

/**
 * Sets up the delivery and returns the descriptor, or a negative value.
 *
 * pthread_sigmask BLOCKS the signals for the whole process - without it SIGINT would keep its
 * default action (immediate death) and the signalfd would never see anything. SIGHUP is in the
 * mask: a terminal hangup must not kill a headless tier server, and a blocked delivery does
 * exactly what SIG_IGN would do.
 *
 * SIGPIPE STANDS THERE FOR THE SAME REASON. The tier runs in a pipeline whose reader is the
 * ase-logview (core/ase-console - it creates the pipe, starts the tier as its child and reads its
 * stdout), and the boot phase writes its progress table to stdout. When the viewer leaves the
 * window while the server still writes, the next write would kill it at once and silently - no
 * core dump, no journal, no shutdown line (logview_main.cpp:951 describes exactly this death).
 * Blocked it cannot kill; the write fails with EPIPE, which is the right way.
 *
 * ONE PLACE FOR ALL FIVE TIERS: without this line, whether SIGPIPE kills a tier would depend on
 * whether one of the libraries it happens to load ignores it - a property by accident of the load
 * list. Set here, it holds for all five tiers alike.
 *
 * THIS LINE STAYS. SIGPIPE is not the console's way of ending the tier. The double Ctrl+C
 * safeguard lives in the CONSOLE (core/ase-console): it holds the first press, it sends SIGTERM
 * on the second, and it keeps the tier in a process group of its own so that no key press hits it
 * directly. Nothing about that stands in this mask.
 */
int open_shutdown_watch() {
    // The argument -1 means "create a new descriptor"; the same value comes back on failure.
    // Both are POSIX and stand here locally, not at file level.
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
 * Picks up a pending stop signal. true means "the operator wants to end".
 *
 * Non-blocking: when nothing is pending, ::read returns -1 with EAGAIN and the tick goes on.
 * SIGHUP and SIGPIPE are read and DISCARDED - they are blocked so that they do not kill, not so
 * that they end. Without this read they would stay in the descriptor.
 *
 * SIGINT COUNTS AS A WISH TO END. The double confirmation of the viewer is right, but its place is
 * the viewer: a tier runs on a VM of its own, under systemd, without keyboard and without console,
 * and a terminal gesture built into it would make it unendable exactly where nobody can trigger
 * the gesture - an SSH or Mosh session in which an operator starts the binary in the foreground
 * would not react to Ctrl+C at all.
 *
 * WHERE THE GESTURE LIVES: in the console that shows it. The ase-logview starts the tier as its
 * own child in a PROCESS GROUP OF ITS OWN (core/ase-console, spawn_in_group) - the terminal
 * delivers Ctrl+C only to the foreground group, so the tier gets none, and on the second press the
 * console sends it SIGTERM. The safeguard hangs on a process group instead of a special rule in a
 * headless service, and it ships with the console binary.
 *
 * SIGHUP AND SIGPIPE STAY DISCARDED, for two different reasons: a terminal hangup must not kill a
 * headless tier, and a closed viewer is no wish to stop - its write error is called EPIPE and is
 * handled where the write happens. Taking them out of the MASK would mean letting them kill;
 * taking them out of this LIST would mean ending on a signal that does not mean ending.
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
 * Gives the delivery back. After the mask is lifted the default action holds again - a second
 * Ctrl+C during teardown ends hard instead of running into a half torn-down App.
 *
 * LIFTING A MASK RESTORES THE DEFAULT ONLY WHERE THE DISPOSITION IS THE DEFAULT. A process started
 * as a background job of a shell without job control inherits SIG_IGN for SIGINT/SIGQUIT across
 * exec (POSIX), and then this emergency exit has no effect. The console starts the tier itself, by
 * fork/exec without an intermediate shell (core/ase-console, spawn_in_group), so no ignoring is
 * inherited and the sentence above holds. It is not the safeguard either: that a single key press
 * does not end the tier is decided by the console's process group, not by its signal mask.
 *
 * BETWEEN arming and teardown a blocked signal becomes pending and is never discarded - SIG_IGN
 * does NOT win there (cross-checked with a signalfd test program).
 */
void close_shutdown_watch(int fd) {
    // SIGPIPE IS LEFT OUT HERE ON PURPOSE AND STAYS BLOCKED. The other three are released so a
    // second Ctrl+C can cut the teardown short - SIGPIPE means no ending, it hits the server
    // exactly when the viewer leaves the window during shutdown. Released, it would kill the
    // tier BEFORE its shutdown lines are in the log file; that is the death without a trace from
    // logview_main.cpp:990. The process ends right after this, so a block left in place costs
    // nothing.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
    ::close(fd);
}

}  // anonymous namespace

// APP IMPLEMENTATION (Orchestrator)

App::App()
    : App(APP_LIFE_PROCESS) {
}

App::App(uint8_t lifecycle)
    : tick_scheduler_(std::make_unique<internal::TickScheduler>())
    , system_registry_(std::make_unique<internal::SystemRegistry>())
    , lifecycle_(lifecycle) {
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
     * BLOCK FIRST, PROCESS LATER - and the separation of these two steps is the whole point. The
     * descriptor comes into being HERE, before the boot; it is read only once the component
     * stands below. A blocked signal is not lost, it stays pending: a Ctrl+C during the boot does
     * not reach the half-built App, but it is picked up at the first tick after the boot and takes
     * the tier down cleanly.
     *
     * WHY THE BLOCK COMES BEFORE THE BOOT: a signal that arrives before the watch reads would
     * otherwise be lost, whatever disposition stood before. And the window is not short: it spans
     * the WHOLE boot with all its retries. A tier whose connection hangs in retry must still end
     * on Ctrl+C.
     *
     * ONLY WHEN THE APP OWNS THE PROCESS. An embedded App (APP_LIFE_HOST, Godot in the Vivarium
     * client) does not touch the signals: the mask holds for the WHOLE process, and there SIGINT,
     * SIGTERM, SIGHUP and SIGPIPE belong to the host and its platform. Blocked and picked up
     * through a signalfd, they would never reach the host again. Without a watch no
     * EcsAppStaShtdComponent comes into being below, so tick() reads no signal and shutdown()
     * closes no descriptor this App did not open itself.
     */
    const bool host_owned = (lifecycle_ == APP_LIFE_HOST);
    // Host-owned the value is never read - the early return below comes before every query.
    const int watch_fd = host_owned ? 0 : open_shutdown_watch();

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
    // The terminal belongs to the host: on_start runs unchanged, only the table is left out.
    // An App without a system draws no table either: a title and a frame with no row under
    // them say nothing the count would not.
    boot_config.render_terminal_table = !host_owned && system_registry_->total_count() > 0;
    internal::print_boot_sequence(*system_registry_, world_, boot_config);

    // Late-System-Registration: if on_start() added new systems (e.g., dlopen modules),
    // re-sort and boot the pending systems. Their on_start() runs in boot_pending_systems.
    if (system_registry_->has_pending()) {
        internal::sort_systems_by_dependencies(*system_registry_);
        internal::boot_pending_systems(*system_registry_, world_, boot_config);
    }

    // Run Lifecycle schedules (Initialization → Configuration) once at startup: systems
    // registered in Initialization or Configuration need their tick() invoked once so effects
    // like MongoDB store_pool() actually fire. Initialization runs before Configuration.
    run_schedule(Schedule::Initialization, 0.0f);
    run_schedule(Schedule::Configuration, 0.0f);

    running_.store(true);
    last_frame_time_ = utils::monotonic_nanos();

    /**
     * ARM the watch, now that the boot is through: Ctrl+C (SIGINT) or a kill (SIGTERM) break the
     * run loop so shutdown() runs the on_stop teardown. SIGHUP is swallowed (a terminal hangup
     * must not kill a headless tier server).
     *
     * BLOCKED ABOVE, READ FROM HERE ON - the two steps are separate on purpose. The component is
     * the switch: only its presence lets App::tick() query the descriptor. A signal from the boot
     * phase lies pending until then and is picked up at the first tick; it reaches no half-built
     * App and is still not lost.
     *
     * WHEN ARMING FAILS, NO COMPONENT COMES INTO BEING - and then it reports itself instead of
     * silently being nothing. Without a watch the server keeps running, but Ctrl+C ends it hard:
     * on_stop is skipped, open connections and buffers are not torn down. That is exactly the
     * kind of failure that would be indistinguishable from "success without work".
     *
     * An embedded App opened no watch above and arms none here; a stop reaches it as a call of
     * shutdown() by the host, never as a signal.
     */
    if (host_owned) {
        return;
    }
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
     * the default disposition (hard exit) instead of stalling on a tearing-down App.
     *
     * COLLECT first, then delete: no entity may be destroyed while a view is iterated. There is
     * exactly one - the vector is the FORM, not the amount.
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
    // An embedded App draws no table and hands the logger its sinks back: the host lives on
    // after shutdown() and keeps logging (unloading the plugins, the next start).
    const bool host_owned = (lifecycle_ == APP_LIFE_HOST);
    // No table for an App without a system - the log line "[Shutdown] sequence entered
    // (0 systems)" still marks the entry into teardown.
    shutdown_config.render_terminal_table = !host_owned && system_registry_->total_count() > 0;
    shutdown_config.restore_log_sinks = host_owned;
    // After the LAST on_stop, before the log replay: the one point where no system runs and the
    // App still stands (set_on_stopped).
    if (on_stopped_callback_ != nullptr) {
        shutdown_config.after_all_stopped = &App::stopped_trampoline;
        shutdown_config.after_all_stopped_user = this;
    }
    internal::print_shutdown_sequence(*system_registry_, world_, shutdown_config);

    running_.store(false);
}

// Introspection API (port of systemRegistry.ts)

size_t App::system_count() const {
    return system_registry_->total_count();
}

uint64_t App::schedule_runs(Schedule schedule) const {
    return tick_scheduler_->runs(schedule);
}

float App::frame_time_max() const {
    return tick_scheduler_->max_frame_time();
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

void App::event_trampoline(void* user) {
    static_cast<App*>(user)->run_game_events();
}

void App::stopped_trampoline(void* user) {
    App* app = static_cast<App*>(user);
    app->on_stopped_callback_(*app);
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

void App::run_game_events() {
    /**
     * THE CALENDAR DECIDES WHEN, THE ECS DECIDES HOW (ARCH_ASE_SCHEDULE section 6.3). The module
     * that computes the date creates one EcsAppReqSchdComponent per crossed boundary; this drain
     * runs between the real-time tiers and Conclusion, so whatever a game-time system writes, the
     * Conclusion of the same tick already sees.
     *
     * The storage is empty in every tick without a crossing; then this costs one size check.
     */
    auto& registry = world_.registry();
    if (registry.storage<EcsAppReqSchdComponent>().size() == 0u) {
        return;
    }

    /**
     * ORDER BY THE CROSSING, NOT BY THE ENTITY. When a long span is caught up, several boundaries
     * fall into one tick - a dawn and the dusk after it, a new TUMBRAE and the season that starts
     * with it - and they fire in the order the calendar crossed them.
     */
    registry.sort<EcsAppReqSchdComponent>(
        [](const EcsAppReqSchdComponent& lhs, const EcsAppReqSchdComponent& rhs) {
            return lhs.sequence < rhs.sequence;
        });

    /**
     * TAKE ALL, DESTROY ALL, THEN RUN. The requests of this tick are taken out before the first
     * schedule runs: a game-time system that creates a request of its own (an event that opens
     * the next one) writes into a storage this loop no longer iterates, and its request fires in
     * the next tick instead of invalidating this iteration.
     */
    ase::containers::Vector<uint32_t> schedules;
    ase::containers::Vector<float> game_dts;
    ase::containers::Vector<Entity> taken;
    for (auto [entity, request] : registry.view<EcsAppReqSchdComponent>().each()) {
        schedules.push_back(request.schedule);
        game_dts.push_back(request.game_dt);
        taken.push_back(entity);
    }
    for (uint32_t i = 0; i < taken.size(); ++i) {
        registry.destroy(taken[i]);
    }

    const uint32_t first_value = static_cast<uint32_t>(Schedule::GameDawn);
    const uint32_t last_value = static_cast<uint32_t>(Schedule::Aevrix);
    for (uint32_t i = 0; i < schedules.size(); ++i) {
        /**
         * A REQUEST FOR A REAL-TIME SCHEDULE IS REFUSED. Its tier runs it already; a second run
         * in the same tick would apply its dt twice and no gate would notice.
         */
        if (schedules[i] < first_value || schedules[i] > last_value) {
            log::error(log::ERR::CAT::INPUT_REJECTED, "App", "run_game_events",
                       "request names no game-time schedule; only values from GameDawn to "
                       "Aevrix fire here");
            continue;
        }
        const Schedule schedule = static_cast<Schedule>(schedules[i]);
        run_schedule(schedule, game_dts[i]);
        tick_scheduler_->note_event_run(schedule);
    }
}

void App::tick(float dt) {
    /**
     * THE SIGNAL PICKUP STANDS IN tick(), NOT IN run(): tick() is the one point EVERY driver of
     * the App passes - the five tier mains through run(), and a test or a tool that calls tick()
     * directly as well - and the quit tag pickup below needs the registry every tick anyway.
     *
     * The view is empty when the watch is not armed; then this costs nothing.
     */
    auto& registry = world_.registry();
    auto watch_view = registry.view<EcsAppStaShtdComponent>();
    for (auto entity : watch_view) {
        if (shutdown_requested(static_cast<int>(watch_view.get<EcsAppStaShtdComponent>(entity).watch_fd))) {
            running_.store(false);
        }
    }

    /**
     * THE SECOND STOP SOURCE BESIDE THE SIGNAL: the quit tag. A system gets the registry and never
     * the App - so only an EVENT the tick picks up can end the process. Whoever is done above L1
     * (KernelCoreStopSystem in Schedule::Conclusion, in the same pass that removes
     * KernelCoreRunnTag) stamps EcsAppQuitReqTag; it is read here, for all five tiers alike. The
     * view is empty as long as nobody stops; then this costs nothing.
     */
    auto quit_view = registry.view<EcsAppQuitReqTag>();
    if (quit_view.begin() != quit_view.end()) {
        running_.store(false);
    }

    // The callbacks are plain function pointers, so no lambda with a capture can stand here: the
    // App goes through them as the opaque user pointer and is recovered at the other end, the
    // same way as everywhere else in the tree.
    tick_scheduler_->tick(dt, &App::schedule_trampoline, &App::event_trampoline, this);
}

void App::run() {
    /**
     * AN EMBEDDED APP MUST NOT RUN, ONLY BE TICKED. run() spins a loop of its own and ends with
     * _exit(0) - in the Vivarium client that would mean: Godot loses its loop, and at the first
     * stop the whole application vanishes without a single destructor. Refused without ending the
     * process and without touching the App: the host broke a contract, the App stays drivable
     * through startup/tick/shutdown unchanged.
     */
    if (lifecycle_ == APP_LIFE_HOST) {
        log::error(log::ERR::CAT::INPUT_REJECTED, "App", "run",
                   "host-owned App is driven through startup/tick/shutdown; run() would take "
                   "the loop and end the process");
        return;
    }

    startup();

    /**
     * PACED TO THE FRAME BAND RATE, NOT FREE-RUNNING - AND THIS IS THE ONE LOOP OF ALL FIVE TIERS.
     * The frame tier has interval = 0.0f in the ticker and runs in EVERY iteration - the loop rate
     * IS the frame band rate. A free-running loop would run the frame band as fast as its own work
     * allows (up to 1000 Hz instead of the 60 declared in schedule.hpp) and burn most of a core on
     * every node, idle just as under load; a loop with a constant dt would carry a time defect of
     * its own.
     *
     * schedule_hz(Schedule::Reception) is the SSOT of the rate: the 60.0f from schedule.hpp drives
     * the pace instead of being unused documentation. dt stays the MEASURED span, never a constant.
     *
     * ABSOLUTE DEADLINES, NOT A RELATIVE REMAINDER: a pacer that sleeps `budget - work` inherits
     * the wake-up latency of the scheduler as a fresh error in EVERY frame and drifts below the
     * nominal rate for good. sleep_until_nanos hands the deadline itself to the kernel; oversleeping
     * one frame shortens the sleep of the next by exactly the same amount - the long-term rate IS
     * the nominal rate. When a tick overruns its budget, the deadline is ANCHORED ANEW instead of
     * caught up: a catch-up would fire several frames without sleep in a row after a stall, and
     * the tier accumulators work with the measured dt anyway - lost time is already booked there.
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
     * THE PROCESS ENDS HERE, AND BEFORE THE STACK DESTRUCTORS. `ARCH_ASE_REP_SRV.md` (section
     * "libdatachannel: _exit(0) after ECS shutdown") prescribes it for every server with
     * libdatachannel WebSockets: the rtc objects block on teardown in EVERY variant -
     * ws_->close(), forceClose(), reset() without close, connections_.clear() - because their
     * destructor waits for the close handshake and the thread join. A `return 0` runs right into
     * that and hangs; the OS cleans up the TCP sockets anyway.
     *
     * WHY THE LINE STANDS HERE AND NOT FIVE TIMES IN THE main.cpp: App::run() has EXACTLY FIVE
     * callers, the five tier mains, and each does nothing after it but `return 0;` - no test, no
     * tool, no client calls it. That makes this the one place where the rule holds for all
     * without repeating it five times.
     *
     * THE LOGGER IS CLOSED FIRST, because _exit skips its buffers as well. Without that the
     * shutdown lines would be lost in exactly the run whose end they are to prove - the same
     * signature (log ends in the middle of a tick, no stop line) by which a strangled tier cannot
     * be told from a cleanly ended one.
     *
     * ctx() STAYS UNCLEARED, and that is intent rather than omission: App::shutdown() runs the
     * on_stop of every system (KernelWbskConnSystem::on_stop calls clear_all()), but leaves the
     * WebSocketResourceManager standing in ctx(). Its destructor is exactly the blocking one this
     * line skips.
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
     * REGION DOMAIN GATE: a node without a region of its own simulates NOTHING - the default of
     * every module and plugin is the SIMULATION plane, and it is held without coverage; only the
     * infrastructure declared in the manifest (APP_PLANE_KERN) always ticks. The gate sits HERE
     * and only here: per system the plane stamped at registration decides (SystemInfo.plane,
     * declared from module.toml `plane` BEFORE loading) - not one system body carries a check of
     * its own, and new modules inherit the default without any entry.
     *
     * The gate is LIVE only where the kernel of the process stamped the EcsAppRgnGateTag anchor
     * (Tier::World - the compute layer); on every other tier the loop is byte-identical to the
     * state without a gate. Coverage is the RegionRect seam row (L0 POD) of the assignment
     * receiver - the SPACE axis and only it: project or tenant filters would be the forbidden silo
     * model (ARCH_ASE_TOPOLOGY.md).
     *
     * Lifecycle schedules (Initialization/Configuration/Termination/Finalization) run for held
     * modules too: manager birth and teardown stay intact, only the running work is held.
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
