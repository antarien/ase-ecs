# ase-ecs

[![Layer](https://img.shields.io/badge/Layer-1%20Core-green.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()

> Entity Component System foundation with Bevy-style scheduling built on EnTT

Part of [ASE - Antares Simulation Engine](../../..)

## Overview

`ase-ecs` is the foundational ECS (Entity Component System) module for ASE. It provides a thin wrapper around the [EnTT](https://github.com/skypjack/entt) library with a Bevy-inspired application builder and named schedule system for organizing game logic execution.

## Features

- **EnTT Wrapper**: Clean C++ API over EnTT's entity-component registry
- **Bevy-Style App Builder**: Declarative system registration with `.add_kernel()`, `.add_module()`, `.add_plugin()`
- **Named Schedules**: Human-readable execution phases (Startup, Update, FixedUpdate, Replication, etc.)
- **System Dependencies**: Run-after constraints for system ordering
- **Fixed Timestep**: Built-in fixed update loop (30Hz) with accumulator
- **Lifecycle Management**: Startup/Shutdown phases with automatic system initialization
- **Schedule Variants**: Pre/Post variants for every schedule (PreUpdate, PostUpdate, etc.)

## Installation

### Dependencies

- C++20 compiler
- EnTT (header-only, included via CMake)

### CMake Integration

```cmake
add_subdirectory(core/core/ase-ecs)
target_link_libraries(your_target PRIVATE ase-ecs)
```

## Usage

### Basic App Setup

```cpp
#include <ase/ecs/system.hpp>
#include <ase/ecs/app.hpp>

// Create application
ase::ecs::App app;

// Add kernel (Layer 2 - required)
app.add_kernel<ase::kernel::Kernel>();

// Add modules (Layer 3)
app.add_module<ase::terrain::TerrainModule>();
app.add_module<ase::player::PlayerModule>();

// Add plugins (Layer 4 - optional)
app.add_plugin<ase::sky::SkyPlugin>();

// Run
app.run();
```

### Creating a System

```cpp
#include <ase/ecs/system.hpp>

class MovementSystem : public ase::ecs::System {
public:
    const char* name() const override { return "MovementSystem"; }

    void tick(ase::ecs::Registry& registry, float dt) override {
        auto view = registry.view<PositionComponent, VelocityComponent>();
        for (auto [entity, pos, vel] : view.each()) {
            pos.x += vel.dx * dt;
            pos.y += vel.dy * dt;
            pos.z += vel.dz * dt;
        }
    }
};
```

### Creating a Module

```cpp
#include <ase/ecs/app.hpp>

struct MovementModule {
    static constexpr const char* name() { return "ase-movement"; }

    void build(ase::ecs::App& app) {
        // Simple registration
        app.add_system<MovementSystem>(ase::ecs::Schedule::Update);

        // With dependencies
        app.add_system_with<CollisionSystem>(ase::ecs::Schedule::FixedUpdate)
            .run_after("PhysicsSystem")
            .with_priority(10);
    }
};
```

### Entity and Component Management

```cpp
// Create entity
auto entity = world.create();

// Add components
world.emplace<PositionComponent>(entity, 0.0f, 0.0f, 0.0f);
world.emplace<VelocityComponent>(entity, 1.0f, 0.0f, 0.0f);

// Query entities
auto view = world.view<PositionComponent, VelocityComponent>();
for (auto [entity, pos, vel] : view.each()) {
    // Process entities
}

// Check component existence
if (world.has<PositionComponent>(entity)) {
    auto& pos = world.get<PositionComponent>(entity);
}

// Remove component
world.remove<VelocityComponent>(entity);

// Destroy entity
world.destroy(entity);
```

## API Reference

### Core Types

```cpp
// Entity handle
using Entity = entt::entity;

// Entity registry
using Registry = entt::registry;

// Null entity constant
constexpr Entity NullEntity = entt::null;
```

### Schedules

Execution order during main loop:

```
First → PreUpdate → FixedUpdate (30Hz loop) → Update → PostUpdate → Replication (20Hz) → Persistence (1Hz) → Last
```

Available schedules:

- **Lifecycle**: `Startup`, `Shutdown`
- **Main Loop**: `First`, `PreUpdate`, `Update`, `PostUpdate`, `Last`
- **Fixed Rate**: `FixedUpdate` (30Hz), `Replication` (20Hz), `Persistence` (1Hz)
- **Variants**: Every schedule has `Pre*` and `Post*` variants

### App API

```cpp
class App {
    // Builder methods
    App& add_kernel<K>();
    App& add_module<M>();
    App& add_plugin<P>();
    App& add_system<S>(Schedule schedule);
    SystemBuilder add_system_with<S>(Schedule schedule);

    // Lifecycle
    void run();                              // Full app lifecycle
    void startup();                          // Initialize systems
    void tick(float dt);                     // Single frame
    void shutdown();                         // Cleanup
    void run_schedule(Schedule s, float dt); // Run specific schedule

    // World access
    World& world();
    Registry& registry();
};
```

### World API

```cpp
class World {
    // Entity management
    Entity create();
    void destroy(Entity e);
    bool valid(Entity e) const;

    // Component management
    template<typename C, typename... Args>
    C& emplace(Entity e, Args&&... args);

    template<typename C>
    C& get(Entity e);

    template<typename C>
    C* try_get(Entity e);

    template<typename C>
    bool has(Entity e) const;

    template<typename C>
    void remove(Entity e);

    // Queries
    template<typename... Components>
    auto view();
};
```

### System Base Class

```cpp
class System {
    virtual const char* name() const = 0;
    virtual void tick(Registry& registry, float dt) = 0;
    virtual void on_start(Registry& registry) {}
    virtual void on_stop(Registry& registry) {}
};
```

## Architecture

### Layer 1 Core Position

`ase-ecs` is a Layer 1 Core module that:

- Depends ONLY on Layer 0 Foundation (ase-math, ase-types)
- Provides ECS infrastructure for Layer 2 Kernel and Layer 3 Modules
- Never depends on higher layers

### Design Principles

1. **Components = Data Only**: No methods, no logic
2. **Systems = Logic Only**: Stateless, no member variables
3. **Views are Cheap**: Create on demand, don't cache
4. **No System-to-System Communication**: Only via components

## Dependencies

- **External**: EnTT (header-only)
- **Internal**: None (Layer 1 Core)

## License

MIT License - Part of ASE (Antares Simulation Engine)
