#pragma once

/**
 * @file schedule.hpp
 * @brief Bevy-inspired named schedule system for ASE
 *
 * Replaces numeric SystemPhase with named schedules.
 * Each schedule can have Pre/Post variants automatically.
 */

#include <cstdint>
#include <string_view>

namespace ase::ecs {

/**
 * Named schedules that define when systems execute.
 * Inspired by Bevy's schedule system.
 *
 * Schedule execution order:
 *   Startup (once) -> MainLoop (repeating) -> Shutdown (once)
 *
 * MainLoop order:
 *   First -> PreUpdate -> FixedUpdateLoop -> Update -> PostUpdate -> Replication -> Persistence -> Last
 */
enum class Schedule : uint32_t {
    // === Lifecycle Schedules ===
    Startup = 0,              // Run once at initialization
    Shutdown = 1,             // Run once at exit (reverse order)

    // === Main Loop Schedules (every frame) ===
    First = 10,               // Very start of each frame
    PreFirst = 11,
    PostFirst = 12,

    PreUpdate = 20,           // Before main update (input processing)
    PrePreUpdate = 21,
    PostPreUpdate = 22,

    Update = 30,              // Main variable-rate update (game logic)
    PreUpdateMain = 31,
    PostUpdateMain = 32,

    PostUpdate = 40,          // After main update (transform propagation)
    PrePostUpdate = 41,
    PostPostUpdate = 42,

    Last = 50,                // Very end of each frame
    PreLast = 51,
    PostLast = 52,

    // === Fixed Timestep Schedules (simulation @ 30Hz default) ===
    FixedFirst = 100,
    PreFixedFirst = 101,
    PostFixedFirst = 102,

    FixedPreUpdate = 110,
    PreFixedPreUpdate = 111,
    PostFixedPreUpdate = 112,

    FixedUpdate = 120,        // Main fixed-timestep simulation (physics, gameplay)
    PreFixedUpdate = 121,
    PostFixedUpdate = 122,

    FixedPostUpdate = 130,
    PreFixedPostUpdate = 131,
    PostFixedPostUpdate = 132,

    FixedLast = 140,
    PreFixedLast = 141,
    PostFixedLast = 142,

    // === Special Purpose Schedules ===
    Replication = 200,        // Network sync @ 20Hz
    PreReplication = 201,
    PostReplication = 202,

    Persistence = 300,        // Database flush @ 1Hz
    PrePersistence = 301,
    PostPersistence = 302,
};


/**
 * Get the Pre-schedule variant for a given schedule
 */
constexpr Schedule pre_schedule(Schedule s) {
    switch (s) {
        case Schedule::First:          return Schedule::PreFirst;
        case Schedule::PreUpdate:      return Schedule::PrePreUpdate;
        case Schedule::Update:         return Schedule::PreUpdateMain;
        case Schedule::PostUpdate:     return Schedule::PrePostUpdate;
        case Schedule::Last:           return Schedule::PreLast;
        case Schedule::FixedFirst:     return Schedule::PreFixedFirst;
        case Schedule::FixedPreUpdate: return Schedule::PreFixedPreUpdate;
        case Schedule::FixedUpdate:    return Schedule::PreFixedUpdate;
        case Schedule::FixedPostUpdate:return Schedule::PreFixedPostUpdate;
        case Schedule::FixedLast:      return Schedule::PreFixedLast;
        case Schedule::Replication:    return Schedule::PreReplication;
        case Schedule::Persistence:    return Schedule::PrePersistence;
        default:                       return s;
    }
}

/**
 * Get the Post-schedule variant for a given schedule
 */
constexpr Schedule post_schedule(Schedule s) {
    switch (s) {
        case Schedule::First:          return Schedule::PostFirst;
        case Schedule::PreUpdate:      return Schedule::PostPreUpdate;
        case Schedule::Update:         return Schedule::PostUpdateMain;
        case Schedule::PostUpdate:     return Schedule::PostPostUpdate;
        case Schedule::Last:           return Schedule::PostLast;
        case Schedule::FixedFirst:     return Schedule::PostFixedFirst;
        case Schedule::FixedPreUpdate: return Schedule::PostFixedPreUpdate;
        case Schedule::FixedUpdate:    return Schedule::PostFixedUpdate;
        case Schedule::FixedPostUpdate:return Schedule::PostFixedPostUpdate;
        case Schedule::FixedLast:      return Schedule::PostFixedLast;
        case Schedule::Replication:    return Schedule::PostReplication;
        case Schedule::Persistence:    return Schedule::PostPersistence;
        default:                       return s;
    }
}

/**
 * Check if a schedule is a Pre-variant
 */
constexpr bool is_pre_schedule(Schedule s) {
    uint32_t val = static_cast<uint32_t>(s);
    // Pre-variants end in 1 (11, 21, 31, 101, 111, 121, 201, 301)
    return (val % 10) == 1 && val > 10;
}

/**
 * Check if a schedule is a Post-variant
 */
constexpr bool is_post_schedule(Schedule s) {
    uint32_t val = static_cast<uint32_t>(s);
    // Post-variants end in 2 (12, 22, 32, 102, 112, 122, 202, 302)
    return (val % 10) == 2 && val > 10;
}

/**
 * Check if a schedule is a fixed-timestep schedule
 */
constexpr bool is_fixed_schedule(Schedule s) {
    uint32_t val = static_cast<uint32_t>(s);
    return val >= 100 && val < 200;
}

/**
 * Get schedule name as string (for logging)
 */
constexpr std::string_view schedule_name(Schedule s) {
    switch (s) {
        case Schedule::Startup:           return "Startup";
        case Schedule::Shutdown:          return "Shutdown";
        case Schedule::First:             return "First";
        case Schedule::PreFirst:          return "PreFirst";
        case Schedule::PostFirst:         return "PostFirst";
        case Schedule::PreUpdate:         return "PreUpdate";
        case Schedule::PrePreUpdate:      return "PrePreUpdate";
        case Schedule::PostPreUpdate:     return "PostPreUpdate";
        case Schedule::Update:            return "Update";
        case Schedule::PreUpdateMain:     return "PreUpdateMain";
        case Schedule::PostUpdateMain:    return "PostUpdateMain";
        case Schedule::PostUpdate:        return "PostUpdate";
        case Schedule::PrePostUpdate:     return "PrePostUpdate";
        case Schedule::PostPostUpdate:    return "PostPostUpdate";
        case Schedule::Last:              return "Last";
        case Schedule::PreLast:           return "PreLast";
        case Schedule::PostLast:          return "PostLast";
        case Schedule::FixedFirst:        return "FixedFirst";
        case Schedule::PreFixedFirst:     return "PreFixedFirst";
        case Schedule::PostFixedFirst:    return "PostFixedFirst";
        case Schedule::FixedPreUpdate:    return "FixedPreUpdate";
        case Schedule::PreFixedPreUpdate: return "PreFixedPreUpdate";
        case Schedule::PostFixedPreUpdate:return "PostFixedPreUpdate";
        case Schedule::FixedUpdate:       return "FixedUpdate";
        case Schedule::PreFixedUpdate:    return "PreFixedUpdate";
        case Schedule::PostFixedUpdate:   return "PostFixedUpdate";
        case Schedule::FixedPostUpdate:   return "FixedPostUpdate";
        case Schedule::PreFixedPostUpdate:return "PreFixedPostUpdate";
        case Schedule::PostFixedPostUpdate:return "PostFixedPostUpdate";
        case Schedule::FixedLast:         return "FixedLast";
        case Schedule::PreFixedLast:      return "PreFixedLast";
        case Schedule::PostFixedLast:     return "PostFixedLast";
        case Schedule::Replication:       return "Replication";
        case Schedule::PreReplication:    return "PreReplication";
        case Schedule::PostReplication:   return "PostReplication";
        case Schedule::Persistence:       return "Persistence";
        case Schedule::PrePersistence:    return "PrePersistence";
        case Schedule::PostPersistence:   return "PostPersistence";
        default:                          return "Unknown";
    }
}

} // namespace ase::ecs

// Hash specialization for Schedule (required for unordered_map)
namespace std {
template<>
struct hash<ase::ecs::Schedule> {
    size_t operator()(ase::ecs::Schedule s) const noexcept {
        return hash<uint32_t>{}(static_cast<uint32_t>(s));
    }
};
} // namespace std
