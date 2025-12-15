#include <ase/ecs/ecs.hpp>
#include <ase/ecs/system_registry.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <algorithm>

namespace ase::ecs {

// Helper: format timestamp like spdlog
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

// Helper: log with consistent format [timestamp] [level] [ASE] message
static void boot_log(const char* level, const std::string& msg) {
    std::cout << "[" << timestamp() << "] [" << level << "] [ASE] " << msg << std::endl;
}

void World::start() {
    const size_t total = systems_.size();
    size_t current = 0;

    std::cout << std::endl;
    boot_log("Inf", "[Booting] Starting " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto& system : systems_) {
        ++current;

        const char* phase_str = phase_name(static_cast<SystemPhase>(system->phase()));

        try {
            system->on_start(registry_);

            std::ostringstream ss;
            ss << "[Booting] [" << phase_str << "] [" << system->name()
               << "] [" << current << "/" << total << "] Started";
            boot_log("Inf", ss.str());
        } catch (const std::exception& e) {
            std::ostringstream ss;
            ss << "[Booting] [" << phase_str << "] [" << system->name()
               << "] [" << current << "/" << total << "] FAILED: " << e.what();
            boot_log("Err", ss.str());
            throw;
        }
    }

    std::cout << std::endl;
    boot_log("Inf", "[Booting] All " + std::to_string(total) + " systems started successfully");
    std::cout << std::endl;
}

void World::stop() {
    const size_t total = systems_.size();
    size_t current = total;

    std::cout << std::endl;
    boot_log("Inf", "[Shutdown] Stopping " + std::to_string(total) + " systems...");
    std::cout << std::endl;

    for (auto it = systems_.rbegin(); it != systems_.rend(); ++it) {
        auto& system = *it;

        const char* phase_str = phase_name(static_cast<SystemPhase>(system->phase()));

        try {
            system->on_stop(registry_);

            std::ostringstream ss;
            ss << "[Shutdown] [" << phase_str << "] [" << system->name()
               << "] [" << current << "/" << total << "] Stopped";
            boot_log("Inf", ss.str());
        } catch (const std::exception& e) {
            std::ostringstream ss;
            ss << "[Shutdown] [" << phase_str << "] [" << system->name()
               << "] [" << current << "/" << total << "] FAILED: " << e.what();
            boot_log("Err", ss.str());
        }

        --current;
    }

    std::cout << std::endl;
    boot_log("Inf", "[Shutdown] All systems stopped");
    std::cout << std::endl;
}

}  // namespace ase::ecs
