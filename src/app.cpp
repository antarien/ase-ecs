#include <ase/ecs/app.hpp>
#include <ase/ecs/ecs.hpp>
#include <ase/ecs/components/app/state/ecs_app_state_rates_component.hpp>
#include <iostream>
#include <thread>

namespace ase::ecs {

App::App() : world_(std::make_unique<World>()) {
    // Initialize timestep configs with defaults
    timesteps_[Schedule::FixedUpdate] = {30.0f, 0.0f, 1.0f / 30.0f};
    timesteps_[Schedule::Replication] = {20.0f, 0.0f, 1.0f / 20.0f};
    timesteps_[Schedule::Persistence] = {1.0f, 0.0f, 1.0f};
}

void App::run() {
    // 1. Run Startup schedule - Kernel creates its entity with AppRatesComponent
    world_->run_startup();

    // 2. Read timestep configuration from components (set by Kernel during Startup)
    read_kernel_config();

    // 3. Enter main loop
    running_.store(true);
    last_frame_time_ = Clock::now();

    std::cout << "\n[App] Entering main loop...\n" << std::endl;

    while (running_.load()) {
        // Calculate frame delta time
        auto now = Clock::now();
        float frame_dt = Duration(now - last_frame_time_).count();
        last_frame_time_ = now;

        // Cap frame time to prevent spiral of death
        if (frame_dt > max_frame_time_) {
            frame_dt = max_frame_time_;
        }

        // Run one frame
        frame(frame_dt);

        // Small sleep to prevent CPU spinning (will be replaced by proper vsync later)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "\n[App] Exiting main loop...\n" << std::endl;

    // 4. Run Shutdown schedule
    world_->run_shutdown();
}

void App::read_kernel_config() {
    // Query for EcsAppStateRatesComponent set by Kernel (or any module) during Startup
    auto view = world_->view<EcsAppStateRatesComponent>();

    for (auto [entity, rates] : view.each()) {
        (void)entity;

        // Update timestep configs from component
        if (rates.fixed_update_hz > 0.0f) {
            timesteps_[Schedule::FixedUpdate].rate_hz = rates.fixed_update_hz;
            timesteps_[Schedule::FixedUpdate].dt = 1.0f / rates.fixed_update_hz;
        }
        if (rates.replication_hz > 0.0f) {
            timesteps_[Schedule::Replication].rate_hz = rates.replication_hz;
            timesteps_[Schedule::Replication].dt = 1.0f / rates.replication_hz;
        }
        if (rates.persistence_hz > 0.0f) {
            timesteps_[Schedule::Persistence].rate_hz = rates.persistence_hz;
            timesteps_[Schedule::Persistence].dt = 1.0f / rates.persistence_hz;
        }
        if (rates.max_frame_time > 0.0f) {
            max_frame_time_ = rates.max_frame_time;
        }

        std::cout << "[App] Read timestep config: "
                  << "FixedUpdate=" << rates.fixed_update_hz << "Hz, "
                  << "Replication=" << rates.replication_hz << "Hz, "
                  << "Persistence=" << rates.persistence_hz << "Hz"
                  << std::endl;

        // Only read from first entity with this component
        break;
    }
}

bool App::should_run_schedule(Schedule schedule, float frame_dt) {
    auto it = timesteps_.find(schedule);
    if (it == timesteps_.end()) {
        return true;  // No config = run every frame
    }

    auto& ts = it->second;
    if (ts.rate_hz <= 0.0f) {
        return true;  // 0 Hz = run every frame
    }

    // Accumulator logic
    ts.accumulator += frame_dt;
    if (ts.accumulator >= ts.dt) {
        ts.accumulator -= ts.dt;
        return true;
    }
    return false;
}

void App::frame(float dt) {
    // === Variable-Rate Schedules (every frame) ===
    world_->run_schedule_with_hooks(Schedule::First, dt);
    world_->run_schedule_with_hooks(Schedule::PreUpdate, dt);

    // === Fixed Timestep: Simulation (30Hz default) ===
    if (should_run_schedule(Schedule::FixedUpdate, dt)) {
        float fixed_dt = timesteps_[Schedule::FixedUpdate].dt;
        world_->run_schedule_with_hooks(Schedule::FixedFirst, fixed_dt);
        world_->run_schedule_with_hooks(Schedule::FixedPreUpdate, fixed_dt);
        world_->run_schedule_with_hooks(Schedule::FixedUpdate, fixed_dt);
        world_->run_schedule_with_hooks(Schedule::FixedPostUpdate, fixed_dt);
        world_->run_schedule_with_hooks(Schedule::FixedLast, fixed_dt);
    }

    // === Variable-Rate Schedules (every frame) ===
    world_->run_schedule_with_hooks(Schedule::Update, dt);
    world_->run_schedule_with_hooks(Schedule::PostUpdate, dt);

    // === Fixed Timestep: Replication (20Hz default) ===
    if (should_run_schedule(Schedule::Replication, dt)) {
        float repl_dt = timesteps_[Schedule::Replication].dt;
        world_->run_schedule_with_hooks(Schedule::Replication, repl_dt);
    }

    // === Fixed Timestep: Persistence (1Hz default) ===
    if (should_run_schedule(Schedule::Persistence, dt)) {
        float pers_dt = timesteps_[Schedule::Persistence].dt;
        world_->run_schedule_with_hooks(Schedule::Persistence, pers_dt);
    }

    // === Variable-Rate Schedules (every frame) ===
    world_->run_schedule_with_hooks(Schedule::Last, dt);
}

}  // namespace ase::ecs
