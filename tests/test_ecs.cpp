/**
 * ASE ECS Module Tests
 */

#include <ase/ecs/system.hpp>
#include <iostream>
#include <cassert>

using namespace ase::ecs;

// Test components
struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Name {
    std::string value;
};

// Test system
class MovementSystem : public System {
public:
    const char* name() const override { return "MovementSystem"; }

    void tick(Registry& registry, float dt) override {
        auto view = registry.view<Position, Velocity>();
        for (auto [entity, pos, vel] : view.each()) {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            pos.z += vel.z * dt;
            ++ticked_;
        }
    }

    int ticked_ = 0;
};

void test_entity_creation() {
    std::cout << "Testing Entity Creation..." << std::endl;

    World world;

    auto e1 = world.create();
    [[maybe_unused]] auto e2 = world.create();

    assert(world.valid(e1));
    assert(world.valid(e2));
    assert(e1 != e2);

    world.destroy(e1);
    assert(!world.valid(e1));
    assert(world.valid(e2));

    std::cout << "  PASSED" << std::endl;
}

void test_components() {
    std::cout << "Testing Components..." << std::endl;

    World world;

    auto entity = world.create();

    // Add components
    world.emplace<Position>(entity, 1.0f, 2.0f, 3.0f);
    world.emplace<Name>(entity, "Player1");

    // Check has
    assert(world.has<Position>(entity));
    assert(world.has<Name>(entity));
    assert(!world.has<Velocity>(entity));

    // Get components
    [[maybe_unused]] auto& pos = world.get<Position>(entity);
    assert(pos.x == 1.0f);
    assert(pos.y == 2.0f);
    assert(pos.z == 3.0f);

    [[maybe_unused]] auto& name = world.get<Name>(entity);
    assert(name.value == "Player1");

    // Try get
    [[maybe_unused]] auto* vel = world.try_get<Velocity>(entity);
    assert(vel == nullptr);

    // Remove component
    world.remove<Name>(entity);
    assert(!world.has<Name>(entity));

    std::cout << "  PASSED" << std::endl;
}

void test_views() {
    std::cout << "Testing Views..." << std::endl;

    World world;

    // Create entities with different components
    auto e1 = world.create();
    world.emplace<Position>(e1, 0.0f, 0.0f, 0.0f);
    world.emplace<Velocity>(e1, 1.0f, 0.0f, 0.0f);

    auto e2 = world.create();
    world.emplace<Position>(e2, 10.0f, 0.0f, 0.0f);
    world.emplace<Velocity>(e2, 0.0f, 1.0f, 0.0f);

    auto e3 = world.create();
    world.emplace<Position>(e3, 20.0f, 0.0f, 0.0f);
    // No velocity

    // View with Position and Velocity
    int count = 0;
    world.view<Position, Velocity>().each([&](auto entity, auto& pos, auto& vel) {
        ++count;
    });
    assert(count == 2);

    // View with just Position
    count = 0;
    world.view<Position>().each([&](auto entity, auto& pos) {
        ++count;
    });
    assert(count == 3);

    std::cout << "  PASSED" << std::endl;
}

void test_systems() {
    std::cout << "Testing Systems..." << std::endl;

    World world;

    // Add system
    auto& movement = world.add_system<MovementSystem>();

    // Create entity
    auto entity = world.create();
    world.emplace<Position>(entity, 0.0f, 0.0f, 0.0f);
    world.emplace<Velocity>(entity, 10.0f, 0.0f, 0.0f);

    // Tick (systems are ticked automatically)
    world.tick(0.1f);

    // Check position changed
    auto& pos = world.get<Position>(entity);
    assert(std::abs(pos.x - 1.0f) < 0.001f);

    assert(movement.ticked_ == 1);

    // Tick again
    world.tick(0.1f);
    assert(std::abs(pos.x - 2.0f) < 0.001f);

    std::cout << "  PASSED" << std::endl;
}

void test_tags() {
    std::cout << "Testing Tags..." << std::endl;

    World world;

    auto e1 = world.create();
    world.emplace<Position>(e1, 0.0f, 0.0f, 0.0f);
    world.emplace<Dirty>(e1);  // Tag

    auto e2 = world.create();
    world.emplace<Position>(e2, 1.0f, 0.0f, 0.0f);
    // Not dirty

    // Count dirty entities
    int dirty_count = 0;
    world.view<Position, Dirty>().each([&](auto entity, auto& pos) {
        ++dirty_count;
    });
    assert(dirty_count == 1);

    // Remove dirty tag
    world.remove<Dirty>(e1);
    dirty_count = 0;
    world.view<Position, Dirty>().each([&](auto entity, auto& pos) {
        ++dirty_count;
    });
    assert(dirty_count == 0);

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== ASE ECS Module Tests ===" << std::endl;

    test_entity_creation();
    test_components();
    test_views();
    test_systems();
    test_tags();

    std::cout << "\n=== All Tests Passed ===" << std::endl;
    return 0;
}
