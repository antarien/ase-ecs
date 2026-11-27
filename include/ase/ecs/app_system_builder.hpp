#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app_system_builder.hpp
 * @design      DSGN_016
 * @brief       SystemBuilder: registers one system with its schedule, its order and its priority
 * @description SystemBuilder is what App::add_system_with hands back: run_after and with_priority
 *              state the order of a system inside its schedule, done() returns to the App, and its
 *              destructor commits the registration. The class is declared here; its inline members
 *              call App::finalize_system and therefore stand in app_unit.hpp right after class App,
 *              where App is complete.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    ecs/module/build
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
 * CORE INFRASTRUCTURE COMPLIANCE
 *
 * [ ] NOT an ECS Component or System
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] No global mutable state (constexpr/const only)
 * [ ] No singletons or static mutable variables
 * [ ] Thread-safe by design (pure functions or explicit mutex)
 * [ ] All public functions documented with @brief, @param, @return
 * [ ] constexpr where possible (compile-time evaluation)
 * [ ] noexcept where possible (no-throw guarantee)
 * [ ] [[nodiscard]] on functions returning values
 * [ ] No magic numbers (use named constants)
 * [ ] No implicit conversions (use explicit constructors)
 * [ ] Header-only OR header+cpp pattern (not mixed)
 * [ ] Include guards via #pragma once
 * [ ] Namespace matches module: ase::{module}
 * [ ] No circular dependencies
 * [ ] No macros (except include guards) - use constexpr/templates
 * [ ] API stable (changes require version bump)
 */

#include "schedule.hpp"
#include "system.hpp"

#include <memory>
#include <string>
#include <ase/containers/vector.hpp>

namespace ase::ecs {

class App;

// SYSTEMBUILDER - fluent API for system configuration

class SystemBuilder {
public:
    SystemBuilder(App& app, Schedule schedule, std::unique_ptr<System> system);

    SystemBuilder& run_after(std::string_view name);
    SystemBuilder& with_priority(int priority);
    App& done();

    // Auto-finalize when destroyed without calling done()
    ~SystemBuilder();

private:
    App& app_;
    Schedule schedule_;
    std::unique_ptr<System> system_;
    ase::containers::Vector<std::string> after_;
    std::string source_;
    std::string version_;
    int priority_ = 0;
    bool finalized_ = false;
};

}  // namespace ase::ecs
