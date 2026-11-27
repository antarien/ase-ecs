#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app_plane.hpp
 * @design      DSGN_016
 * @brief       The two planes a system ticks on: region-bound simulation and infrastructure
 * @description The plane decides whether a system ticks without region coverage: APP_PLANE_SIM for
 *              region-bound simulation, APP_PLANE_KERN for infrastructure that always ticks.
 *              App::declare_module_plane takes one of them per module, App::add_kernel declares
 *              the kernel APP_PLANE_KERN, and App::run_schedule holds what is APP_PLANE_SIM while
 *              no RegionRect coverage stands.
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

#include <cstdint>

namespace ase::ecs {

// Planes of the region domain gate (App::run_schedule). EXACTLY TWO, operator decision of
// 2026-08-26: the default of every module and plugin is SIMULATION - without RegionRect coverage
// it is held, because a node without active chunks simulates NOTHING (VIS_ASE.md: chunk model and
// sphere frequencies; weather and celestial are simulation too and run only with coverage, in
// their slow schedules). The one exception is the infrastructure, which is no simulation. The
// declaration comes DATA-DRIVEN from the module manifest (module.toml `plane`, declared by the
// kernel before loading), never from code. The gate works on the SPACE axis (coverage) only -
// never on project or tenant axes: the silo model is forbidden (ARCH_ASE_TOPOLOGY.md), the graph
// is ONE, and wanderers change their simulator by region handoff (the ReplicaHoffFlipSystem
// chain), never their state.
constexpr uint8_t APP_PLANE_SIM  = 0;  // region-bound simulation (default of every module)
constexpr uint8_t APP_PLANE_KERN = 1;  // infrastructure: always ticks (network, persistence, kernel)

}  // namespace ase::ecs
