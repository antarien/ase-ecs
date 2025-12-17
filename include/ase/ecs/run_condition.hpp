#pragma once

/**
 * @file run_condition.hpp
 * @brief Run conditions for conditional system execution
 *
 * Bevy-inspired run conditions that determine whether a system should execute.
 */

#include <functional>
#include <vector>
#include <algorithm>
#include <optional>

#include <entt/entt.hpp>

namespace ase::ecs {

// Forward declaration
using Registry = entt::registry;

/**
 * A run condition is a function that returns true if a system should run.
 * Called before each system tick.
 */
using RunCondition = std::function<bool(const Registry&)>;

/**
 * Built-in run conditions
 */
namespace conditions {

/**
 * Always run (default behavior)
 */
inline RunCondition always() {
    return [](const Registry&) { return true; };
}

/**
 * Never run (for disabling systems)
 */
inline RunCondition never() {
    return [](const Registry&) { return false; };
}

/**
 * Run if a resource exists in the registry context
 */
template<typename T>
RunCondition resource_exists() {
    return [](const Registry& reg) {
        return reg.ctx().contains<T>();
    };
}

/**
 * Run if a resource equals a specific value
 */
template<typename T>
RunCondition resource_equals(const T& value) {
    return [value](const Registry& reg) {
        const auto* res = reg.ctx().find<T>();
        return res && *res == value;
    };
}

/**
 * Run if any entity has a specific component
 */
template<typename C>
RunCondition any_with_component() {
    return [](const Registry& reg) {
        auto view = reg.view<C>();
        return !view.empty();
    };
}

/**
 * Run if no entity has a specific component
 */
template<typename C>
RunCondition none_with_component() {
    return [](const Registry& reg) {
        auto view = reg.view<C>();
        return view.empty();
    };
}

/**
 * Run every N ticks (uses internal counter)
 */
inline RunCondition every_n_ticks(uint32_t n) {
    return [n, counter = uint32_t(0)](const Registry&) mutable {
        return (++counter % n) == 0;
    };
}

/**
 * Run only on the first tick
 */
inline RunCondition run_once() {
    return [ran = false](const Registry&) mutable {
        if (ran) return false;
        ran = true;
        return true;
    };
}

/**
 * Combine conditions with AND (all must be true)
 */
inline RunCondition all_of(std::initializer_list<RunCondition> conditions) {
    std::vector<RunCondition> conds(conditions);
    return [conds = std::move(conds)](const Registry& reg) {
        return std::all_of(conds.begin(), conds.end(),
            [&](const auto& c) { return c(reg); });
    };
}

/**
 * Combine conditions with OR (any must be true)
 */
inline RunCondition any_of(std::initializer_list<RunCondition> conditions) {
    std::vector<RunCondition> conds(conditions);
    return [conds = std::move(conds)](const Registry& reg) {
        return std::any_of(conds.begin(), conds.end(),
            [&](const auto& c) { return c(reg); });
    };
}

/**
 * Negate a condition
 */
inline RunCondition not_condition(RunCondition condition) {
    return [cond = std::move(condition)](const Registry& reg) {
        return !cond(reg);
    };
}

/**
 * Run only when a state has changed (compares with previous value)
 */
template<typename T>
RunCondition on_change() {
    return [prev = std::optional<T>()](const Registry& reg) mutable {
        const auto* current = reg.ctx().find<T>();
        if (!current) return false;

        bool changed = !prev.has_value() || *prev != *current;
        prev = *current;
        return changed;
    };
}

/**
 * Run when entering a specific state value
 */
template<typename T>
RunCondition on_enter(const T& state) {
    return [state, prev = std::optional<T>()](const Registry& reg) mutable {
        const auto* current = reg.ctx().find<T>();
        if (!current) return false;

        bool entering = (!prev.has_value() || *prev != state) && *current == state;
        prev = *current;
        return entering;
    };
}

/**
 * Run when exiting a specific state value
 */
template<typename T>
RunCondition on_exit(const T& state) {
    return [state, prev = std::optional<T>()](const Registry& reg) mutable {
        const auto* current = reg.ctx().find<T>();

        bool exiting = prev.has_value() && *prev == state &&
                       (!current || *current != state);
        prev = current ? std::optional<T>(*current) : std::nullopt;
        return exiting;
    };
}

} // namespace conditions

} // namespace ase::ecs
