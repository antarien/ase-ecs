#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        app_assembly.hpp
 * @design      DSGN_016
 * @brief       The contracts a kernel, a module and a plugin meet to be assembled into an App
 * @description Assembly is the step before the first tick: App::add_kernel, add_module and
 *              add_plugin hand each unit its build() and collect the systems it registers. What a
 *              unit must offer for that stands here: the concepts Kernel, Module and Plugin, and
 *              detail::get_version, which reads a unit's version() where it has one.
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

#include <string>
#include <type_traits>

namespace ase::ecs {

class App;

// VERSION DETECTION HELPER

namespace detail {

/* THE DETECTION STANDS WHERE IT IS USED, as one requires expression: can T::version() be
 * written. No trait type, no partial specialisation, no void_t - the house form of this file,
 * the same the concepts Kernel, Module and Plugin below are built in. `if constexpr` chooses at
 * compile time: whoever has version() gets it, whoever has not, the empty string.
 */

// Get version if available, otherwise return empty string
template<typename T>
const char* get_version() {
    if constexpr (requires { T::version(); }) {
        return T::version();
    } else {
        return "";
    }
}

}  // namespace detail

// KERNEL CONCEPT (Layer 2) - core kernel, foundation for modules/plugins

template<typename T>
concept Kernel = requires(T kernel, App& app) {
    { kernel.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// MODULE CONCEPT (Layer 3) - core game systems, statically linked

template<typename T>
concept Module = requires(T module, App& app) {
    { module.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

// PLUGIN CONCEPT (Layer 4) - optional, hot-loadable features

template<typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
    { T::name() } -> std::convertible_to<const char*>;
};

}  // namespace ase::ecs
