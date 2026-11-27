#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app_lifecycle.hpp
 * @design      DSGN_016
 * @brief       Who owns the process an App runs in: the App itself or an embedding host
 * @description Who owns the process decides the lifecycle: APP_LIFE_PROCESS for a tier server whose
 *              main() is the App and spins run(), APP_LIFE_HOST for an embedding host that calls
 *              startup, tick and shutdown itself. App(uint8_t) takes one of them and keeps it for
 *              the life of the App.
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

// Who owns the PROCESS this App runs in. EXACTLY TWO cases, both with a place in the tree: the
// five tier servers, whose main() IS the App - it blocks SIGINT/SIGTERM/SIGHUP/SIGPIPE for the
// whole process, spins its own loop in run() and ends the process with _exit(0); and an embedding
// host that owns process, loop, signals and terminal (Godot in the Vivarium client,
// PLAN_ASE_VIVARIUM_PHASE_00_CONTRACT section 00.1). There the App is a guest: startup/tick/
// shutdown on call, no signal watch, no change of the signal mask, no progress table, no run().
// The simulation itself is the same in both cases.
constexpr uint8_t APP_LIFE_PROCESS = 0;  // the App owns the process (tier server, default)
constexpr uint8_t APP_LIFE_HOST    = 1;  // a host owns the process (embedded App)

}  // namespace ase::ecs
