/**
 * ASE ECS Module Tests
 *
 * WHY THIS FILE USES CHECK AND NOT assert() - measured 2026-08-20:
 *
 *   All five compile_commands.json of the servers carry -O3 -DNDEBUG; 141 of 141 test
 *   translation units, not one without. Under NDEBUG assert() expands to nothing, so all
 *   21 checks in this file were empty statements in the built configuration. It printed
 *   "All Tests Passed" while checking nothing, and main() returned 0 unconditionally.
 *
 *   The four [[maybe_unused]] markers that stood on pos, name, vel and e2 were the SYMPTOM:
 *   those variables were unused precisely because their only use sat inside a compiled-away
 *   assert. They are gone with the cause - CHECK uses the variables for real.
 *
 * WHY NOT doctest, which 97 of 113 module test files use: this target links only ase::ecs
 * (core/ase-ecs/CMakeLists.txt:131-133), it has no doctest dependency. Adding one is a build
 * wiring decision, not something to slip into a test conversion - it is reported as an open
 * item instead. CHECK below is the form for files without doctest; it survives NDEBUG, names
 * the failing expression with its line, and main() returns the failure count.
 *
 * ALMOST NOTHING HERE IS DECIDABLE AT COMPILE TIME, unlike the component tests in other
 * modules: Position and Velocity are local test structs with NO default member initialisers
 * (`float x, y, z;`), so there is no zero-initialisation to state. Only the Tag is - see the
 * single static_assert below. Forcing more would mean inventing properties this file does
 * not test.
 *
 * IF A CHECK TURNS RED ON THE FIRST REAL RUN, THAT IS A FINDING, NOT A REGRESSION. The checks
 * were never executed; a failure appearing now was already failing, silently, for as long as
 * the build has carried -DNDEBUG. Do not "fix" it by weakening the check.
 */

#include <ase/ecs/system.hpp>
// app.hpp joined system.hpp on 2026-08-20: World's system container was deleted, and App is
// where systems are registered and run - here as in the 1725 module and plugin files that
// already did it this way. This test was the only caller World's container ever had.
#include <ase/ecs/app.hpp>
// Core tag - moved out of system.hpp on 2026-08-22, needs its own include now
#include <ase/ecs/components/tag/ecs_dty_tag.hpp>
#include <cmath>
#include <iostream>
#include <type_traits>

using namespace ase::ecs;

// ============================================================================
// Test Utilities
// ============================================================================

namespace {
int g_failures = 0;
}  // namespace

// The replacement for assert(): NOT compiled away under NDEBUG, and it reports the
// expression that failed together with its line instead of only aborting.
#define CHECK(expr) do { \
    if (!(expr)) { \
        ++g_failures; \
        std::cout << "\n    FAIL " << __FILE__ << ":" << __LINE__ << "  " #expr; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    std::cout << "Running " #fn "... "; \
    const int before = g_failures; \
    fn(); \
    std::cout << (g_failures == before ? "OK\n" : "\n  FAILED\n"); \
} while(0)

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

// A Tag carries no data - the one property of this file decidable while building. The type itself
// comes from the include at the top of this file, NOT from a local struct: it is the real core tag
// ase::ecs::EcsDtyTag, so this assertion states something about production code rather than about a
// test-local copy that could drift away from it.
static_assert(std::is_empty_v<EcsDtyTag>);

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
        }
    }
};

// `int ticked_ = 0;` STOOD HERE AND IS GONE. It was a member variable in a System — the exact
// thing the STATELESS rule forbids, sitting in the ECS's own test as an example to copy.
// The assertion it carried, CHECK(movement.ticked_ == 1), was already subsumed: the same test
// checks that x moves from 0 to 1.0 after one tick and to 2.0 after the second. A counter that
// only confirms what the position already proves is not coverage, it is a second thermometer.

void test_entity_creation() {
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

void test_components() {
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

void test_views() {
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

void test_systems() {
    // App, not World: World's system container was deleted on 2026-08-20 and this test was its
    // only caller. Registration now names a Schedule instead of an implicit int phase, which is
    // what production does everywhere - Dynamics is the movement tier (30 Hz, fixed step).
    App app;
    app.add_system<MovementSystem>(Schedule::Dynamics);

    // Create entity
    Registry& registry = app.registry();
    auto entity = registry.create();
    registry.emplace<Position>(entity, 0.0f, 0.0f, 0.0f);
    registry.emplace<Velocity>(entity, 10.0f, 0.0f, 0.0f);

    // Run the one schedule the system sits in
    app.run_schedule(Schedule::Dynamics, 0.1f);

    // Check position changed
    auto& pos = registry.get<Position>(entity);
    CHECK(std::abs(pos.x - 1.0f) < 0.001f);

    // Run again
    app.run_schedule(Schedule::Dynamics, 0.1f);
    CHECK(std::abs(pos.x - 2.0f) < 0.001f);
}

void test_tags() {
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

int main() {
    std::cout << "=== ASE ECS Module Tests ===" << std::endl;

    RUN_TEST(test_entity_creation);
    RUN_TEST(test_components);
    RUN_TEST(test_views);
    RUN_TEST(test_systems);
    RUN_TEST(test_tags);

    // The exit code is the result. The old main() returned 0 unconditionally and printed
    // "All Tests Passed" whether or not anything had been checked - which, under NDEBUG,
    // was never.
    if (g_failures != 0) {
        std::cout << "\n=== " << g_failures << " check(s) FAILED ===\n";
        return 1;
    }
    std::cout << "\n=== All Tests Passed ===" << std::endl;
    return 0;
}
