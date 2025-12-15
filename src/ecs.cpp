#include <ase/ecs/ecs.hpp>
#include <ase/ecs/system_registry.hpp>
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
              << "[" << color << level << "\x1b[0m] [ASE] " << msg << std::endl;
}

static void log_phase(const char* stage, const char* phase, const char* name, size_t cur, size_t total, const char* status) {
    std::ostringstream ss;
    ss << "[" << stage << "] [" << phase << "] [" << name << "] [" << cur << "/" << total << "] " << status;
    log_msg("Inf", ss.str());
}

static void log_phase_err(const char* stage, const char* phase, const char* name, size_t cur, size_t total, const char* err) {
    std::ostringstream ss;
    ss << "[" << stage << "] [" << phase << "] [" << name << "] [" << cur << "/" << total << "] FAILED: " << err;
    log_msg("Err", ss.str());
}

void World::start() {
    const size_t total = systems_.size();
    size_t current = 0;

    std::cout << std::endl;
    log_msg("Inf", "[Booting] Starting " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto& system : systems_) {
        ++current;
        const char* phase = phase_name(static_cast<SystemPhase>(system->phase()));

        try {
            system->on_start(registry_);
            log_phase("Booting", phase, system->name(), current, total, "Started");
        } catch (const std::exception& e) {
            log_phase_err("Booting", phase, system->name(), current, total, e.what());
            throw;
        }
    }

    std::cout << std::endl;
    log_msg("Inf", "[Booting] All " + std::to_string(total) + " systems started successfully");
    std::cout << std::endl;
}

void World::stop() {
    const size_t total = systems_.size();
    size_t current = total;

    std::cout << std::endl;
    log_msg("Inf", "[Shutdown] Stopping " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto it = systems_.rbegin(); it != systems_.rend(); ++it) {
        auto& system = *it;
        const char* phase = phase_name(static_cast<SystemPhase>(system->phase()));

        try {
            system->on_stop(registry_);
            log_phase("Shutdown", phase, system->name(), current, total, "Stopped");
        } catch (const std::exception& e) {
            log_phase_err("Shutdown", phase, system->name(), current, total, e.what());
        }
        --current;
    }

    std::cout << std::endl;
    log_msg("Inf", "[Shutdown] All systems stopped");
    std::cout << std::endl;
}

}  // namespace ase::ecs
