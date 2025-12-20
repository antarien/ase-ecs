#pragma once

/**
 * EcsBootstrapStateSpawnRequestComponent - Request to spawn a sub-system
 *
 * Systems create entities with this component to request spawning of sub-systems.
 * The EcsBootstrapProcessorSystem processes these requests and adds the systems.
 */

#include <ase/ecs/schedule.hpp>
#include <functional>
#include <memory>

namespace ase::ecs {

class System;

struct EcsBootstrapStateSpawnRequestComponent {
    Schedule target_schedule = Schedule::Update;
    std::function<std::unique_ptr<System>()> factory = nullptr;
};

}  // namespace ase::ecs
