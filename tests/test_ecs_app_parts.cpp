/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        test_ecs_app_parts.cpp
 * @design      DSGN_016
 * @brief       Pins that the five part headers of App carry App on their own, without app.hpp
 * @description app.hpp is being cut into five part headers of one purpose each: app_assembly.hpp
 *              (the concepts Kernel, Module, Plugin and detail::get_version), app_lifecycle.hpp
 *              (APP_LIFE_PROCESS, APP_LIFE_HOST), app_plane.hpp (APP_PLANE_SIM, APP_PLANE_KERN),
 *              app_system_builder.hpp (SystemBuilder) and app_unit.hpp (class App). app.hpp becomes
 *              the collector of the five once the write on it is permitted; until then it still
 *              carries the same definitions itself, token for token (comments aside).
 *
 *              THIS FILE INCLUDES THE FIVE PARTS AND NEVER app.hpp. A unit that included both would
 *              see every name twice and fail to compile, so the parts can only be compiled in a unit
 *              of their own, and this target is that unit. It proves, before app.hpp hands over to
 *              them, what the hand-over relies on:
 *
 *              1. EACH PART STANDS ON ITS OWN. app_unit.hpp pulls the four smaller parts, and the
 *                 constants, the concepts and get_version are read here through them.
 *              2. THE App OF app_unit.hpp RUNS. A module assembled through add_module registers its
 *                 system through add_system_with (SystemBuilder), and the system ticks over its
 *                 schedule.
 *              3. THE PLANE DECIDES. Where the kernel stamped the region gate anchor
 *                 (EcsAppRgnGateTag) and no RegionRect coverage stands, a module of APP_PLANE_SIM is
 *                 held and a module declared APP_PLANE_KERN ticks.
 *
 *              EVERY CHECK HAS ITS POSITIVE CONTROL IN THIS FILE: get_version is shown to give the
 *              empty string for a unit without version(), the position is read before the first
 *              pass, and the held module is shown to tick once the gate anchor is gone. A check that
 *              can never see anything would otherwise pass every case.
 *
 *              ITS OWN TARGET, NOT A SECOND SOURCE OF ase-ecs-test: that target includes app.hpp,
 *              and it switches the doctest entry point on for every translation unit
 *              (tests/CMakeLists.txt says so).
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/validation/check
 * @usecase     usecase:100357 - run the tick loop
 * @actor       Entity Component System (ECS)
 * @created     2026-11-25
 * @modified    2026-11-25
 * @version     00.01.25.00075 [feat]
 *
 * Copyright (c) Jan Ohlmann. All rights reserved.
 * Licensed to ANTARES SIMULACAO LTDA. Use is governed by LICENSE.md and PJV.md in this repository.
 * SPDX-License-Identifier: LicenseRef-ASE-LIC
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

#include <ase/ecs/app_assembly.hpp>
#include <ase/ecs/app_lifecycle.hpp>
#include <ase/ecs/app_plane.hpp>
#include <ase/ecs/app_system_builder.hpp>
#include <ase/ecs/app_unit.hpp>
#include <ase/ecs/components/app/tag/ecs_app_rgn_gate_tag.hpp>
#include <ase/ecs/system.hpp>

#include <string_view>

namespace ase::ecs {

// Test fixtures

struct PartsPosition {
    float x = 0.0f;
};

struct PartsVelocity {
    float x = 0.0f;
};

// Moves the position by its velocity once per pass. It carries no member variable: a System is
// stateless, and the position the cases read proves how often it ticked.
class PartsMoveSystem : public ecs::System {
public:
    const char* name() const override { return "PartsMoveSystem"; }

    void tick(Registry& registry, float dt) override {
        for (auto [entity, pos, vel] : registry.view<PartsPosition, PartsVelocity>().each()) {
            (void)entity;
            pos.x += vel.x * dt;
        }
    }
};

// A module that registers its system through add_system_with, the path SystemBuilder serves.
struct PartsSimModule {
    static constexpr const char* name() { return "ase-parts-sim"; }
    static constexpr const char* version() { return "00.00.00.00001"; }
    void build(App& app) { app.add_system_with<PartsMoveSystem>(Schedule::Dynamics).done(); }
};

// The same module under a name of its own, declared infrastructure before it is assembled.
struct PartsKernModule {
    static constexpr const char* name() { return "ase-parts-kern"; }
    void build(App& app) { app.add_system_with<PartsMoveSystem>(Schedule::Dynamics).done(); }
};

static_assert(Module<PartsSimModule>);
static_assert(Module<PartsKernModule>);
static_assert(APP_PLANE_SIM != APP_PLANE_KERN);
static_assert(APP_LIFE_PROCESS != APP_LIFE_HOST);

// Test cases
//
// EACH NAME CARRIES THE IDENTIFIER THE CASE IS SEARCHED BY - test_app_parts - so a search for that
// identifier lands here.

TEST_CASE("test_app_parts - get_version of app_assembly.hpp reads version() where a unit has one") {
    CHECK(std::string_view(detail::get_version<PartsSimModule>()) == "00.00.00.00001");
    // Positive control: a unit without version() gives the empty string, not the other unit's text.
    CHECK(std::string_view(detail::get_version<PartsKernModule>()).empty());
}

TEST_CASE("test_app_parts - the App of app_unit.hpp ticks the system a module registers") {
    App app;
    app.add_module<PartsSimModule>();

    Registry& registry = app.registry();
    const auto entity = registry.create();
    registry.emplace<PartsPosition>(entity, 0.0f);
    registry.emplace<PartsVelocity>(entity, 10.0f);

    // Positive control: nothing moved before the first pass.
    CHECK(registry.get<PartsPosition>(entity).x == doctest::Approx(0.0f));

    app.run_schedule(Schedule::Dynamics, 0.1f);
    CHECK(registry.get<PartsPosition>(entity).x == doctest::Approx(1.0f));
}

TEST_CASE("test_app_parts - without coverage the plane of app_plane.hpp holds SIM and runs KERN") {
    App app;
    app.declare_module_plane(PartsKernModule::name(), APP_PLANE_KERN);
    app.add_module<PartsSimModule>();
    app.add_module<PartsKernModule>();

    Registry& registry = app.registry();
    const auto gate = registry.create();
    registry.emplace<EcsAppRgnGateTag>(gate);

    const auto entity = registry.create();
    registry.emplace<PartsPosition>(entity, 0.0f);
    registry.emplace<PartsVelocity>(entity, 10.0f);

    // Gate live, no RegionRect row: only the KERN module's system moves the position, once.
    app.run_schedule(Schedule::Dynamics, 0.1f);
    CHECK(registry.get<PartsPosition>(entity).x == doctest::Approx(1.0f));

    // Positive control: with the anchor gone the gate is off, and the held SIM module ticks too.
    registry.remove<EcsAppRgnGateTag>(gate);
    app.run_schedule(Schedule::Dynamics, 0.1f);
    CHECK(registry.get<PartsPosition>(entity).x == doctest::Approx(3.0f));
}

}  // namespace ase::ecs
