#include <ase/ecs/app.hpp>
#include <ase/log/log.hpp>
#include <ase/ecs/internal/boot_logger.hpp>
#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/ecs/internal/shutdown_sequence.hpp>
#include <ase/ecs/internal/tick_scheduler.hpp>

#include <thread>

namespace ase::ecs {

// =============================================================================
// APP IMPLEMENTATION (Orchestrator)
// =============================================================================

App::App()
    : tick_scheduler_(std::make_unique<internal::TickScheduler>())
    , system_registry_(std::make_unique<internal::SystemRegistry>()) {
}

App::~App() = default;

void App::finalize_system(Schedule schedule, std::unique_ptr<System> system,
                          std::vector<std::string> after, int priority,
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
    // No separate run_schedule(Initialization/Configuration) needed — all systems already
    // ran on_start() during print_boot_sequence + boot_pending_systems.
    if (system_registry_->has_pending()) {
        internal::sort_systems_by_dependencies(*system_registry_);
        internal::boot_pending_systems(*system_registry_, world_, boot_config);
    }

    running_.store(true);
    last_frame_time_ = Clock::now();
}

void App::shutdown() {
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

const std::vector<internal::SystemInfo>& App::system_infos() const {
    return system_registry_->infos();
}

const std::vector<std::unique_ptr<System>>& App::systems_for(Schedule schedule) const {
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
            log::debug("[App::run_schedule] tick: {}", system->name());
            system->tick(world_.registry(), dt);
        }
    }
}

}  // namespace ase::ecs
