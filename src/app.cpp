#include <ase/ecs/app.hpp>
#include <ase/log/log.hpp>
#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/tick_scheduler.hpp>

#include <atomic>
#include <csignal>
#include <thread>

namespace ase::ecs {

namespace {

// Graceful-shutdown signal handling (downstrap counterpart to the boot sequence).
// SIGINT/SIGTERM flip the running App's loop flag to false so App::run() (or a server's
// explicit while(is_running()) loop) exits and App::shutdown() runs on_stop for every
// system. Installed centrally in App::startup() so ALL tier servers (world/reasoning/
// replica/engine) get the teardown — restoring the per-main handler removed in the
// "streamline main entry point" refactor (world commit 104af81). Only an async-signal-safe
// atomic store happens in the handler.
std::atomic<App*> g_signal_app{nullptr};

void on_terminate_signal(int /*signum*/) {
    App* app = g_signal_app.load(std::memory_order_acquire);
    if (app != nullptr) app->quit();
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

void App::startup() {
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
    last_frame_time_ = Clock::now();

    // Install the graceful-shutdown handler now that boot has completed: Ctrl+C (SIGINT) or
    // a kill (SIGTERM) breaks the run loop so shutdown() runs the on_stop downstrap. SIGHUP is
    // ignored (terminal hangup must not kill a headless tier server). Registered last so a
    // signal during boot cannot reach a half-built App.
    g_signal_app.store(this, std::memory_order_release);
    std::signal(SIGINT, on_terminate_signal);
    std::signal(SIGTERM, on_terminate_signal);
    std::signal(SIGHUP, SIG_IGN);
}

void App::shutdown() {
    // Detach the signal handler before teardown so a second Ctrl+C during shutdown reverts to
    // the default disposition (hard exit) instead of re-entering quit() on a tearing-down App.
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    g_signal_app.store(nullptr, std::memory_order_release);

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

void App::tick(float dt) {
    tick_scheduler_->tick(dt, [this](Schedule schedule, float sched_dt) {
        run_schedule(schedule, sched_dt);
    });
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
    auto& systems = system_registry_->systems_for(schedule);
    for (auto& system : systems) {
        if (system && system->enabled()) {
            system->tick(world_.registry(), dt);
        }
    }
}

}  // namespace ase::ecs
