/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        test_ecs.cpp
 * @brief       Pins entity lifetime, component storage, views, tags and system execution of ase-ecs
 * @description ASE ECS Module Tests
 *
 *              WHY THIS FILE USES CHECK AND NOT assert(): the server builds compile their test
 *              translation units with -O3 -DNDEBUG. Under NDEBUG assert() expands to nothing, so
 *              an assert-based check is an empty statement in the built configuration and a run
 *              reports success while checking nothing.
 *
 *              doctest carries the checks. The test target has its own tests/CMakeLists.txt, and
 *              that is what pulls doctest in. CHECK survives NDEBUG, names the failing EXPRESSION
 *              and BOTH VALUES rather than only the line, counts cases and assertions separately,
 *              and the run's exit status is the number of failed assertions. An entry point
 *              written by hand would bring back exactly the class doctest removes: a main() that
 *              returns 0 whether or not anything was checked.
 *
 *              ALMOST NOTHING HERE IS DECIDABLE AT COMPILE TIME, unlike the component tests in
 *              other modules: Position and Velocity are local test structs with NO default
 *              member initialisers (`float x, y, z;`), so there is no zero-initialisation to
 *              state. Only the Tag is - see the single static_assert below. Forcing more would
 *              mean inventing properties this file does not test.
 *
 *              A CHECK THAT TURNS RED IS A FINDING, NOT A REGRESSION. Do not "fix" it by
 *              weakening the check.
 *
 * META
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/validation/check
 * @design      DSGN_016
 * @usecase     usecase:101396 - assure engine component quality
 * @actor       Engine Unit Test (ENGUTEST)
 * @created     2026-08-20
 * @modified    2026-11-05
 * @version     3.0.1
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <doctest/doctest.h>

#include <ase/ecs/system.hpp>
// App is where systems are registered and run - here as in the module and plugin files.
#include <ase/ecs/app.hpp>
// Core tag EcsDtyTag, declared in its own header.
#include <ase/ecs/components/tag/ecs_dty_tag.hpp>
// Layer 0: ase::math::abs carries the two float comparisons in the system case below. The
// engine reaches std:: arithmetic through ase-math and through nothing else, so a test spelling
// it directly would be teaching the form it exists to pin.
#include <ase/math/math.hpp>
#include <type_traits>

/**
 * THE CASES LIVE IN THE NAMESPACE OF WHAT THEY TEST, rather than next to it behind a global
 * `using namespace ase::ecs`. Such a using pulls EVERY name of the module into the scope that
 * also holds doctest and the standard library; a future name collision then hits a file that
 * has nothing to do with the matter.
 *
 * The second reason is the more important one here, and it concerns the base class below:
 * `ecs::System` resolves INSIDE ase::ecs because the lookup for `ecs` reaches the enclosing
 * namespace ase and finds ase::ecs there. The inheritance therefore stands under its full name
 * - which is the form a reader, and the rule that permits this one inheritance and no other,
 * recognises. Under a global `using namespace ase::ecs` it would read `System`.
 */
namespace ase::ecs {

// Test fixtures

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Name {
    std::string value;
};

// A Tag carries no data - the one property of this file decidable while building. The type itself
// comes from the include at the top of this file, NOT from a local struct: it is the real core tag
// ase::ecs::EcsDtyTag, so this assertion states something about production code rather than about a
// test-local struct of the same name that could drift away from it.
static_assert(std::is_empty_v<EcsDtyTag>);

// Test system. It carries no member variable: a System is stateless, and the position the cases
// below read already proves how often it ticked.
class MovementSystem : public ecs::System {
public:
    const char* name() const override { return "MovementSystem"; }

    void tick(Registry& registry, float dt) override {
        auto view = registry.view<Position, Velocity>();
        for (auto [entity, pos, vel] : view.each()) {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            pos.z += vel.z * dt;
        }
    }
};

// Test cases
//
// EACH NAME CARRIES THE IDENTIFIER THE CASE IS SEARCHED BY - test_components and the others - so
// a search for that identifier lands here.

TEST_CASE("test_entity_creation - create, compare and destroy entities") {
    World world;

    auto e1 = world.create();
    auto e2 = world.create();

    CHECK(world.valid(e1));
    CHECK(world.valid(e2));
    CHECK(e1 != e2);

    world.destroy(e1);
    CHECK(!world.valid(e1));
    CHECK(world.valid(e2));
}

TEST_CASE("test_components - emplace, has, get, try_get and remove") {
    World world;

    auto entity = world.create();

    // Add components
    world.emplace<Position>(entity, 1.0f, 2.0f, 3.0f);
    world.emplace<Name>(entity, "Player1");

    // Check has
    CHECK(world.has<Position>(entity));
    CHECK(world.has<Name>(entity));
    CHECK(!world.has<Velocity>(entity));

    // Get components
    auto& pos = world.get<Position>(entity);
    CHECK(pos.x == 1.0f);
    CHECK(pos.y == 2.0f);
    CHECK(pos.z == 3.0f);

    auto& name = world.get<Name>(entity);
    CHECK(name.value == "Player1");

    // Try get
    auto* vel = world.try_get<Velocity>(entity);
    CHECK(vel == nullptr);

    // Remove component
    world.remove<Name>(entity);
    CHECK(!world.has<Name>(entity));
}

TEST_CASE("test_views - a view sees exactly the entities carrying its component set") {
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

    // View with Position and Velocity. The lambda parameters stay UNNAMED rather than being
    // cast to (void): the loop counts rows, it does not read them.
    int count = 0;
    world.view<Position, Velocity>().each([&](auto, auto&, auto&) {
        ++count;
    });
    CHECK(count == 2);

    // View with just Position
    count = 0;
    world.view<Position>().each([&](auto, auto&) {
        ++count;
    });
    CHECK(count == 3);
}

TEST_CASE("test_systems - a registered system ticks over its schedule and moves the position") {
    // Systems are registered on App under a Schedule, the form production uses everywhere -
    // Dynamics is the movement tier (30 Hz, fixed step).
    App app;
    app.add_system<MovementSystem>(Schedule::Dynamics);

    // Create entity
    Registry& registry = app.registry();
    auto entity = registry.create();
    registry.emplace<Position>(entity, 0.0f, 0.0f, 0.0f);
    registry.emplace<Velocity>(entity, 10.0f, 0.0f, 0.0f);

    // Run the one schedule the system sits in
    app.run_schedule(Schedule::Dynamics, 0.1f);

    // Check position changed. The tolerance stays ABSOLUTE: doctest::Approx would compare
    // RELATIVELY, and at the second sample (2.0f) that silently doubles the window this test
    // was written with.
    auto& pos = registry.get<Position>(entity);
    CHECK(ase::math::abs(pos.x - 1.0f) < 0.001f);

    // Run again
    app.run_schedule(Schedule::Dynamics, 0.1f);
    CHECK(ase::math::abs(pos.x - 2.0f) < 0.001f);
}

TEST_CASE("test_tags - a tag-filtered view sees only tagged entities, before and after removal") {
    World world;

    auto e1 = world.create();
    world.emplace<Position>(e1, 0.0f, 0.0f, 0.0f);
    world.emplace<EcsDtyTag>(e1);  // Tag

    auto e2 = world.create();
    world.emplace<Position>(e2, 1.0f, 0.0f, 0.0f);
    // Not dirty

    // Count dirty entities
    int dirty_count = 0;
    world.view<Position, EcsDtyTag>().each([&](auto, auto&) {
        ++dirty_count;
    });
    CHECK(dirty_count == 1);

    // Remove dirty tag
    world.remove<EcsDtyTag>(e1);
    dirty_count = 0;
    world.view<Position, EcsDtyTag>().each([&](auto, auto&) {
        ++dirty_count;
    });
    CHECK(dirty_count == 0);
}

}  // namespace ase::ecs
