#pragma once

/**
 * EcsAppStateRatesComponent - Timestep configuration for App main loop
 *
 * Higher layers (Kernel, Modules) emplace this on an entity during Startup.
 * App queries for it after Startup to configure its accumulators.
 */

namespace ase::ecs {

struct EcsAppStateRatesComponent {
    float fixed_update_hz = 0.0f;
    float replication_hz = 0.0f;
    float persistence_hz = 0.0f;
    float max_frame_time = 0.0f;
};

}  // namespace ase::ecs
