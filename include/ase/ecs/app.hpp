#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app.hpp
 * @brief       Application assembly and the tick loop that drives it, a pure collector of its part headers
 * @description Declares App, the object a process builds a world with: kernel,
 *              modules and plugins register their systems into it, and it runs
 *              the schedules through TickScheduler. Everything above it
 *              registers systems, everything below it stores or sorts them.
 *              The header ratchet (WRFL_ASE_TIER_WIPE, rule 6) dedicated it: every
 *              purpose stands in a part header of its own, and this file holds only
 *              their includes, so a change to one purpose compiles only its includers.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    ecs/module
 * @design      DSGN_016
 * @usecase     usecase:100357 - run the tick loop
 * @actor       Entity Component System (ECS)
 * @created     2025-12-01
 * @modified    2026-11-25
 * @version     00.01.25.00064 [feat]
 *
 * Copyright (c) Jan Ohlmann. All rights reserved.
 * Licensed to ANTARES SIMULACAO LTDA. Use is governed by LICENSE.md and PJV.md in this repository.
 * SPDX-License-Identifier: LicenseRef-ASE-LIC
 *
 * Usage:
 *   ecs::App()
 *       .add_kernel<ase::kernel::Kernel>()
 *       .add_module<PlayerModule>()
 *       .add_plugin<SkyPlugin>()
 *       .run();
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

#include "app_assembly.hpp"
#include "app_lifecycle.hpp"
#include "app_plane.hpp"
#include "app_system_builder.hpp"
#include "app_unit.hpp"
