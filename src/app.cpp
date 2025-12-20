#include <ase/ecs/app.hpp>
#include <iostream>
#include <thread>

namespace ase::ecs {

App::App() = default;

void App::run() {
    // 1. Run Startup schedule
    run_schedule(Schedule::Startup, 0.0f);

    // 2. Enter main loop
    running_.store(true);
    last_frame_time_ = Clock::now();

    while (running_.load()) {
        auto now = Clock::now();
        float frame_dt = Duration(now - last_frame_time_).count();
        last_frame_time_ = now;

        if (frame_dt > max_frame_time_) {
            frame_dt = max_frame_time_;
        }

        // First
        run_schedule(Schedule::First, frame_dt);

        // PreUpdate
        run_schedule(Schedule::PreUpdate, frame_dt);

        // FixedUpdate (30Hz)
        fixed_accumulator_ += frame_dt;
        while (fixed_accumulator_ >= fixed_dt_) {
            run_schedule(Schedule::FixedFirst, fixed_dt_);
            run_schedule(Schedule::FixedPreUpdate, fixed_dt_);
            run_schedule(Schedule::FixedUpdate, fixed_dt_);
            run_schedule(Schedule::FixedPostUpdate, fixed_dt_);
            run_schedule(Schedule::FixedLast, fixed_dt_);
            fixed_accumulator_ -= fixed_dt_;
        }

        // Update
        run_schedule(Schedule::Update, frame_dt);

        // PostUpdate
        run_schedule(Schedule::PostUpdate, frame_dt);

        // Replication (20Hz)
        replication_accumulator_ += frame_dt;
        if (replication_accumulator_ >= replication_dt_) {
            run_schedule(Schedule::Replication, replication_dt_);
            replication_accumulator_ -= replication_dt_;
        }

        // Persistence (1Hz)
        persistence_accumulator_ += frame_dt;
        if (persistence_accumulator_ >= persistence_dt_) {
            run_schedule(Schedule::Persistence, persistence_dt_);
            persistence_accumulator_ -= persistence_dt_;
        }

        // Last
        run_schedule(Schedule::Last, frame_dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 3. Run Shutdown schedule
    run_schedule(Schedule::Shutdown, 0.0f);
}

void App::run_schedule(Schedule schedule, float dt) {
    auto it = schedule_systems_.find(schedule);
    if (it == schedule_systems_.end()) {
        return;
    }

    for (auto& system : it->second) {
        if (system->enabled()) {
            system->tick(world_.registry(), dt);
        }
    }
}

}  // namespace ase::ecs
