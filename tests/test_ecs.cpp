/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        test_ecs.cpp
 * @brief       Pins entity lifetime, component storage, views, tags and system execution of ase-ecs
 * @description ASE ECS Module Tests
 *
 *              WHY THIS FILE USES CHECK AND NOT assert() - measured 2026-08-20:
 *
 *              All five compile_commands.json of the servers carry -O3 -DNDEBUG; 141 of 141
 *              test translation units, not one without. Under NDEBUG assert() expands to
 *              nothing, so all 21 checks in this file were empty statements in the built
 *              configuration. It printed "All Tests Passed" while checking nothing, and main()
 *              returned 0 unconditionally.
 *
 *              The four [[maybe_unused]] markers that stood on pos, name, vel and e2 were the
 *              SYMPTOM: those variables were unused precisely because their only use sat inside
 *              a compiled-away assert. They are gone with the cause - CHECK uses the variables
 *              for real.
 *
 *              WHY NOT doctest, which 97 of 113 module test files use - THIS PARAGRAPH IS THE
 *              OPEN ITEM, AND IT IS CLOSED AS OF 2026-09-01. It read, verbatim: "this target
 *              links only ase::ecs (core/ase-ecs/CMakeLists.txt:131-133), it has no doctest
 *              dependency. Adding one is a build wiring decision, not something to slip into a
 *              test conversion - it is reported as an open item instead. CHECK below is the
 *              form for files without doctest; it survives NDEBUG, names the failing expression
 *              with its line, and main() returns the failure count."
 *
 *              THE LINE NUMBERS INSIDE THAT QUOTE ARE THE ONES OF 2026-08-20 AND NO LONGER
 *              RESOLVE. They are quoted, not followed: the block they addressed is exactly the
 *              one this change rewrote, so the very act of closing the item aged the pointer
 *              that described it. The link target lives on as text and must not be "repaired"
 *              to a current number - a quote that silently tracks its source stops being one.
 *
 *              The finding was correct and the wiring is now built rather than reported.
 *              core/ase-ecs was measured to be the ONLY core module whose test target sat
 *              inline in the module CMakeLists; its seven siblings - codegen, console, convert,
 *              markdown, neo4j, serial, vault - all carry their own tests/CMakeLists.txt. This
 *              file has one now, and it is what pulls doctest in.
 *
 *              WHAT THE SECOND CONVERSION TAKES WITH IT, without losing a single check: the
 *              hand-written CHECK macro, the RUN_TEST macro, the global g_failures counter and
 *              the six std::cout lines are gone. doctest does every one of those and more - it
 *              names the failing EXPRESSION and BOTH VALUES rather than only the line, it
 *              counts cases and assertions separately, and the run's exit status is the number
 *              of failed assertions.
 *
 *              THE TWO GRAVESTONES THAT GO WITH THEM, because both describe a class that comes
 *              back the moment someone writes an entry point by hand. On the CHECK macro: it
 *              was "NOT compiled away under NDEBUG, and it reports the expression that failed
 *              together with its line instead of only aborting." On main(): "The old main()
 *              returned 0 unconditionally and printed 'All Tests Passed' whether or not
 *              anything had been checked - which, under NDEBUG, was never."
 *
 *              ALMOST NOTHING HERE IS DECIDABLE AT COMPILE TIME, unlike the component tests in
 *              other modules: Position and Velocity are local test structs with NO default
 *              member initialisers (`float x, y, z;`), so there is no zero-initialisation to
 *              state. Only the Tag is - see the single static_assert below. Forcing more would
 *              mean inventing properties this file does not test.
 *
 *              IF A CHECK TURNS RED ON THE FIRST REAL RUN, THAT IS A FINDING, NOT A REGRESSION.
 *              The checks were never executed; a failure appearing now was already failing,
 *              silently, for as long as the build has carried -DNDEBUG. Do not "fix" it by
 *              weakening the check.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/validation/check
 * @created     2026-08-20
 * @modified    2026-09-01
 * @version     3.0.0
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
// app.hpp joined system.hpp on 2026-08-20: World's system container was deleted, and App is
// where systems are registered and run - here as in the 1725 module and plugin files that
// already did it this way. This test was the only caller World's container ever had.
#include <ase/ecs/app.hpp>
// Core tag - moved out of system.hpp on 2026-08-22, needs its own include now
#include <ase/ecs/components/tag/ecs_dty_tag.hpp>
// Layer 0: ase::math::abs replaces the std::abs of the two float comparisons in the system
// case below. The engine reaches std:: arithmetic through ase-math and through nothing else,
// so a test spelling it directly would be teaching the form it exists to pin.
#include <ase/math/math.hpp>
// <iostream> is gone with the six std::cout lines it served; doctest owns the reporting now.
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
 * recognises. Under `using namespace ase::ecs` it was spelled `System`.
 */
namespace ase::ecs {

// ============================================================================
// Test fixtures
// ============================================================================

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

// `int ticked_ = 0;` STOOD HERE AND IS GONE. It was a member variable in a System — the exact
// thing the STATELESS rule forbids, sitting in the ECS's own test as an example to copy.
// The assertion it carried, CHECK(movement.ticked_ == 1), was already subsumed: the same test
// checks that x moves from 0 to 1.0 after one tick and to 2.0 after the second. A counter that
// only confirms what the position already proves is not coverage, it is a second thermometer.

// ============================================================================
// Test cases
//
// EACH NAME CARRIES THE IDENTIFIER OF THE FUNCTION IT REPLACED. The five cases were free
// functions driven by a RUN_TEST macro; doctest registers them instead. Keeping test_components
// and its siblings inside the case name means a search for the old identifier still lands here
// - a rename that only a diff can follow is a loss even when no check is dropped.
// ============================================================================

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
