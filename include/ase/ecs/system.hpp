#pragma once

/**
 * ASE ECS Module
 *
 * Thin wrapper around EnTT providing the Entity Component System foundation.
 * All game objects (chunks, entities, players) are ECS entities with components.
 *
 * Usage:
 *   #include <ase/ecs/ecs.hpp>
 *
 *   ase::ecs::World world;
 *   auto entity = world.create();
 *   world.emplace<Position>(entity, 0.0f, 0.0f, 0.0f);
 *   world.emplace<Velocity>(entity, 1.0f, 0.0f, 0.0f);
 *
 *   // Systems iterate over entities with specific components
 *   world.view<Position, Velocity>().each([](auto entity, auto& pos, auto& vel) {
 *       pos.x += vel.x * dt;
 *   });
 */

#include <entt/entt.hpp>
#include <cstdint>
#include <string>
#include <functional>
#include <type_traits>
#include <algorithm>
#include <memory>
#include <vector>

namespace ase::ecs {

// ============================================================================
// Core Type Aliases
// ============================================================================

/// Entity handle
using Entity = entt::entity;

/// Null entity constant
inline constexpr Entity NullEntity = entt::null;

/// Entity registry (the "world")
using Registry = entt::registry;

/// Component storage traits
template<typename Component>
using Storage = entt::storage<Component>;

// ============================================================================
// View Types (for querying entities)
// ============================================================================

/// View with specific components (read/write)
template<typename... Components>
using View = entt::view<entt::get_t<Components...>>;

// ============================================================================
// System Base
// ============================================================================

/**
 * Base class for ECS systems
 *
 * Systems contain logic that operates on entities with specific components.
 * Override tick() to implement system behavior.
 *
 * Systems are registered via the App builder:
 *   app.add_system<MySystem>(Schedule::FixedUpdate);
 */
class System {
public:
    virtual ~System() = default;

    /// System name for debugging/profiling
    [[nodiscard]] virtual const char* name() const = 0;

    /// Called every frame/tick
    virtual void tick(Registry& registry, float dt) = 0;

    /// Called on system startup
    virtual void on_start(Registry& /*registry*/) {}

    /// Called on system shutdown
    virtual void on_stop(Registry& /*registry*/) {}

    /// System phase (for boot order) - set by SystemRegistry
    [[nodiscard]] int phase() const { return phase_; }
    void set_phase(int phase) { phase_ = phase; }

    /// System priority within phase (lower = runs first)
    [[nodiscard]] virtual int priority() const { return 0; }

    /// Dependencies (system names that must start before this one)
    [[nodiscard]] virtual std::vector<std::string> dependencies() const { return {}; }

    /// Is system enabled?
    [[nodiscard]] bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
    int phase_ = 0;
};

// ============================================================================
// World (Registry + Systems)
// ============================================================================

/**
 * The World contains all entities and runs systems
 *
 * This is the central object that owns the ECS registry and manages
 * system execution order.
 */
class World {
public:
    World() = default;
    ~World() = default;

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // ========================================================================
    // Entity Management
    // ========================================================================

    /// Create a new entity
    [[nodiscard]] Entity create() {
        return registry_.create();
    }

    /// Create entity with hint (for predictable IDs)
    [[nodiscard]] Entity create(Entity hint) {
        return registry_.create(hint);
    }

    /// Destroy an entity
    void destroy(Entity entity) {
        registry_.destroy(entity);
    }

    /// Check if entity is valid
    [[nodiscard]] bool valid(Entity entity) const {
        return registry_.valid(entity);
    }

    /// Get entity count
    [[nodiscard]] size_t size() const {
        return registry_.storage<Entity>()->size();
    }

    // ========================================================================
    // Component Management
    // ========================================================================

    /// Add component to entity
    template<typename Component, typename... Args>
    decltype(auto) emplace(Entity entity, Args&&... args) {
        if constexpr (std::is_empty_v<Component>) {
            registry_.emplace<Component>(entity, std::forward<Args>(args)...);
        } else {
            return registry_.emplace<Component>(entity, std::forward<Args>(args)...);
        }
    }

    /// Add or replace component
    template<typename Component, typename... Args>
    Component& emplace_or_replace(Entity entity, Args&&... args) {
        return registry_.emplace_or_replace<Component>(entity, std::forward<Args>(args)...);
    }

    /// Get component (throws if missing)
    template<typename Component>
    [[nodiscard]] Component& get(Entity entity) {
        return registry_.get<Component>(entity);
    }

    template<typename Component>
    [[nodiscard]] const Component& get(Entity entity) const {
        return registry_.get<Component>(entity);
    }

    /// Try get component (returns nullptr if missing)
    template<typename Component>
    [[nodiscard]] Component* try_get(Entity entity) {
        return registry_.try_get<Component>(entity);
    }

    template<typename Component>
    [[nodiscard]] const Component* try_get(Entity entity) const {
        return registry_.try_get<Component>(entity);
    }

    /// Check if entity has component
    template<typename Component>
    [[nodiscard]] bool has(Entity entity) const {
        return registry_.all_of<Component>(entity);
    }

    /// Check if entity has all components
    template<typename... Components>
    [[nodiscard]] bool has_all(Entity entity) const {
        return registry_.all_of<Components...>(entity);
    }

    /// Remove component from entity
    template<typename Component>
    void remove(Entity entity) {
        registry_.remove<Component>(entity);
    }

    // ========================================================================
    // Views (Queries)
    // ========================================================================

    /// Get view of entities with components
    template<typename... Components>
    [[nodiscard]] auto view() {
        return registry_.view<Components...>();
    }

    template<typename... Components>
    [[nodiscard]] auto view() const {
        return registry_.view<Components...>();
    }

    /// Get view excluding certain components
    template<typename... Include, typename... Exclude>
    [[nodiscard]] auto view_exclude() {
        return registry_.view<Include...>(entt::exclude<Exclude...>);
    }

    // ========================================================================
    // Signals/Events
    // ========================================================================

    /// Register callback for component construction
    template<typename Component>
    auto on_construct() {
        return registry_.on_construct<Component>();
    }

    /// Register callback for component destruction
    template<typename Component>
    auto on_destroy() {
        return registry_.on_destroy<Component>();
    }

    /// Register callback for component update
    template<typename Component>
    auto on_update() {
        return registry_.on_update<Component>();
    }

    // ========================================================================
    // System Management
    // ========================================================================

    /// Add a system by type
    template<typename T, typename... Args>
    T& add_system(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        systems_.push_back(std::move(system));
        sort_systems();
        return ref;
    }

    /// Add a system by pointer (for auto-registration)
    System& add_system_ptr(std::unique_ptr<System> system) {
        System& ref = *system;
        systems_.push_back(std::move(system));
        sort_systems();
        return ref;
    }

    /// Get system by type (returns nullptr if not found)
    template<typename T>
    T* get_system() {
        for (auto& sys : systems_) {
            if (auto* ptr = dynamic_cast<T*>(sys.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    /// Tick all systems
    void tick(float dt) {
        for (auto& system : systems_) {
            if (system->enabled()) {
                system->tick(registry_, dt);
            }
        }
    }

    /// Tick systems up to and including a specific phase
    void tick_up_to_phase(int max_phase, float dt) {
        for (auto& system : systems_) {
            if (system->enabled() && system->phase() <= max_phase) {
                system->tick(registry_, dt);
            }
        }
    }

    /// Tick only systems in a specific phase range
    void tick_phase_range(int min_phase, int max_phase, float dt) {
        for (auto& system : systems_) {
            if (system->enabled() &&
                system->phase() >= min_phase &&
                system->phase() <= max_phase) {
                system->tick(registry_, dt);
            }
        }
    }

    /// Tick only systems in a specific phase
    void tick_phase(int phase, float dt) {
        tick_phase_range(phase, phase, dt);
    }

    /// Get system count
    [[nodiscard]] size_t system_count() const { return systems_.size(); }

    /// Get systems (for logging)
    [[nodiscard]] const std::vector<std::unique_ptr<System>>& systems() const { return systems_; }

    // ========================================================================
    // Direct Registry Access
    // ========================================================================

    /// Get underlying registry (for advanced use)
    [[nodiscard]] Registry& registry() { return registry_; }
    [[nodiscard]] const Registry& registry() const { return registry_; }

private:
    void sort_systems() {
        std::sort(systems_.begin(), systems_.end(),
            [](const auto& a, const auto& b) {
                return a->priority() < b->priority();
            });
    }

    Registry registry_;
    std::vector<std::unique_ptr<System>> systems_;
};

// ============================================================================
// Common Component Tags
// ============================================================================

/// Tag for entities that need to be deleted
struct Destroy {};

/// Tag for dirty/modified entities
struct Dirty {};

/// Tag for newly created entities
struct Created {};

/// Tag for entities with pending network sync
struct PendingSync {};

// ============================================================================
// Command Message (for Client→Server commands via ECS pattern)
// ============================================================================

/**
 * Generic command message component.
 *
 * Server creates entity with this component from REST API.
 * Plugin systems query for their command type and process.
 * System destroys entity after processing.
 *
 * Example:
 *   POST /api/command { "type": "sky_time", "hour": 6.0 }
 *   → Entity with CommandMessage { type="sky_time", payload="{\"hour\":6.0}" }
 *   → SkyTimeSystem queries for type=="sky_time", processes, destroys
 */
struct CommandMessage {
    std::string type;     // Command type (e.g. "sky_time", "terrain_mutate")
    std::string payload;  // JSON payload for plugin to parse
};

// ============================================================================
// Entity ID Utilities
// ============================================================================

/// Get numeric ID from entity
[[nodiscard]] inline uint32_t entity_id(Entity entity) {
    return static_cast<uint32_t>(entt::to_integral(entity));
}

/// Get entity version
[[nodiscard]] inline uint32_t entity_version(Entity entity) {
    return entt::to_version(entity);
}

}  // namespace ase::ecs
