#include <ase/ecs/ecs.hpp>
#include <ase/ecs/system_registry.hpp>
#include <ase/ecs/schedule_registry.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace ase::ecs {

static std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

static void log_msg(const char* level, const std::string& msg) {
    const char* color = (level[0] == 'E') ? "\x1b[38;5;167m" : "\x1b[38;5;71m";
    std::cout << "\x1b[38;5;242m[" << timestamp() << "]\x1b[0m "
              << "[" << color << level << "\x1b[0m] [ASE] [SERVER] " << msg << std::endl;
}

static void log_info(const std::string& msg) { log_msg("INF", msg); }
static void log_err(const std::string& msg) { log_msg("ERR", msg); }

static void log_phase(const char* stage, const char* phase, const char* name, size_t cur, size_t total, const char* status) {
    std::ostringstream ss;
    ss << "[" << stage << "] [" << phase << "] [" << cur << "/" << total << "] [" << name << "] " << status;
    log_info(ss.str());
}

static void log_phase_err(const char* stage, const char* phase, const char* name, size_t cur, size_t total, const char* err) {
    std::ostringstream ss;
    ss << "[" << stage << "] [" << phase << "] [" << cur << "/" << total << "] [" << name << "] FAILED: " << err;
    log_err(ss.str());
}

void World::start() {
    // Create all systems with the beautiful bootstrap display
    SystemRegistry::create_all_systems(*this);

    const size_t total = systems_.size();
    size_t current = 0;

    std::cout << std::endl;
    log_info("[Booting] Starting " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto& system : systems_) {
        ++current;
        // Use schedule name for logging
        std::string schedule_str = "System";

        try {
            system->on_start(registry_);
            log_phase("Booting", schedule_str.c_str(), system->name(), current, total, "Started");
        } catch (const std::exception& e) {
            log_phase_err("Booting", schedule_str.c_str(), system->name(), current, total, e.what());
            throw;
        }
    }

    std::cout << std::endl;
    log_info("[Booting] All " + std::to_string(total) + " systems started successfully");
    std::cout << std::endl;
}

void World::stop() {
    const size_t total = systems_.size();
    size_t current = total;

    std::cout << std::endl;
    log_info("[Shutdown] Stopping " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto it = systems_.rbegin(); it != systems_.rend(); ++it) {
        auto& system = *it;
        std::string schedule_str = "System";

        try {
            system->on_stop(registry_);
            log_phase("Shutdown", schedule_str.c_str(), system->name(), current, total, "Stopped");
        } catch (const std::exception& e) {
            log_phase_err("Shutdown", schedule_str.c_str(), system->name(), current, total, e.what());
        }
        --current;
    }

    std::cout << std::endl;
    log_info("[Shutdown] All systems stopped");
    std::cout << std::endl;
}

// ============================================================================
// Schedule-Based Execution
// ============================================================================

void World::initialize_schedule(Schedule schedule) {
    if (initialized_schedules_.count(schedule)) {
        return;  // Already initialized
    }

    auto descriptors = ScheduleRegistry::get_systems(schedule);
    auto& systems_for_schedule = schedule_systems_[schedule];

    for (const auto* desc : descriptors) {
        // Create the system instance
        if (desc->factory) {
            auto system = desc->factory();
            System* raw_ptr = system.get();

            // Add to main systems list
            systems_.push_back(std::move(system));

            // Track in schedule map
            systems_for_schedule.push_back({raw_ptr, desc});
        }
    }

    initialized_schedules_.insert(schedule);
}

void World::run_schedule(Schedule schedule, float dt) {
    // Initialize if needed
    if (!initialized_schedules_.count(schedule)) {
        initialize_schedule(schedule);
    }

    auto it = schedule_systems_.find(schedule);
    if (it == schedule_systems_.end()) {
        return;  // No systems for this schedule
    }

    for (auto& [system, desc] : it->second) {
        // Check if system is enabled
        if (!system->enabled()) {
            continue;
        }

        // Check all run conditions
        bool should_run = true;
        for (const auto& condition : desc->run_conditions) {
            if (!condition(registry_)) {
                should_run = false;
                break;
            }
        }

        if (should_run) {
            system->tick(registry_, dt);
        }
    }
}

void World::run_schedule_with_hooks(Schedule schedule, float dt) {
    run_schedule(pre_schedule(schedule), dt);
    run_schedule(schedule, dt);
    run_schedule(post_schedule(schedule), dt);
}

void World::run_fixed_update(float frame_dt, float fixed_dt) {
    fixed_accumulator_ += frame_dt;

    while (fixed_accumulator_ >= fixed_dt) {
        run_schedule_with_hooks(Schedule::FixedFirst, fixed_dt);
        run_schedule_with_hooks(Schedule::FixedPreUpdate, fixed_dt);
        run_schedule_with_hooks(Schedule::FixedUpdate, fixed_dt);
        run_schedule_with_hooks(Schedule::FixedPostUpdate, fixed_dt);
        run_schedule_with_hooks(Schedule::FixedLast, fixed_dt);

        fixed_accumulator_ -= fixed_dt;
    }
}

void World::run_startup() {
    if (startup_ran_) {
        return;  // Only run once
    }

    log_info("[Schedule] Running Startup schedule...");
    run_schedule(Schedule::Startup, 0.0f);
    startup_ran_ = true;
    log_info("[Schedule] Startup schedule complete");
}

void World::run_shutdown() {
    if (shutdown_ran_) {
        return;  // Only run once
    }

    log_info("[Schedule] Running Shutdown schedule...");
    run_schedule(Schedule::Shutdown, 0.0f);
    shutdown_ran_ = true;
    log_info("[Schedule] Shutdown schedule complete");
}

}  // namespace ase::ecs
