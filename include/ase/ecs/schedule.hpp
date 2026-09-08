#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        schedule.hpp
 * @brief       Bevy-inspired named schedule system for ASE
 * @description Defines 66 schedules across 21 frequency tiers for 5000+ systems.
 *              42 Real-Time schedules (Lifecycle to Diurnal) plus 24 Game-Time
 *              schedules based on the Antarian Calendar (Lexicon Temporis).
 *              See ARCH_ASE_SCHEDULE.md for complete documentation.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    property/temporal/schedule/timing
 * @created     2025-01-01
 * @modified    2026-08-20
 * @version     2.0.0
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   #include <ase/ecs/schedule.hpp>
 *   app.add_system<MySystem>(ase::ecs::Schedule::Dynamics);
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

// =============================================================================
// SCHEDULE ENUM
// =============================================================================

/**
 * @brief Named schedules that define when systems execute
 *
 * Inspired by Bevy's schedule system. 66 schedules across 21 frequency tiers.
 *
 * Schedule execution order:
 *   Initialization (once) → MainLoop (repeating) → Finalization (once)
 *
 * MainLoop contains multiple frequency tiers from ~60Hz down to 1/day,
 * plus Game-Time schedules triggered by the Antarian Calendar.
 *
 * Real-Time Schedules (42):
 *   Lifecycle (4), Frame (5), Kinetic (3), Reactive (2), Tactical (4),
 *   Adaptive (4), Progressive (3), Cyclic (3), Gradual (2), Incremental (2),
 *   Ambient (2), Periodic (2), Epochal (2), Extended (2), Diurnal (2)
 *
 * Game-Time Schedules (24):
 *   GameTumbrae (3), GameNovendrix (3), GameLunbrex (2), GameConiunctrix (2),
 *   GameTemporix (5), GameStellar (3), GameVectum (2), GameTenebrax (1),
 *   GameAetrix (2), GameAevrix (1)
 */
enum class Schedule : uint8_t {
    // =========================================================================
    // Lifecycle (once)
    // =========================================================================
    Initialization = 0,       // Module setup, manager creation
    Configuration = 1,        // Cross-module initialization
    Termination = 2,          // Graceful shutdown preparation
    Finalization = 3,         // Final cleanup

    // =========================================================================
    // Frame (~60Hz, variable)
    // =========================================================================
    Reception = 10,           // Buffer drain, event clearing
    Ingestion = 11,           // Deserialization, data parsing
    Integration = 12,         // Commands, API processing
    Production = 13,          // Visual sync, render preparation
    Conclusion = 14,          // Logging, debug output

    // =========================================================================
    // Kinetic (30Hz) - Fast physics simulation
    // =========================================================================
    Dynamics = 20,            // Forces, velocity, acceleration
    Kinematics = 21,          // Position, movement, transforms
    Collision = 22,           // Collision detection, resolution

    // =========================================================================
    // Reactive (20Hz) - Network synchronization
    // =========================================================================
    Transmission = 30,        // Network send, broadcast
    Synchronization = 31,     // State sync, replication

    // =========================================================================
    // Tactical (10Hz) - Fast AI and perception
    // =========================================================================
    Perception = 40,          // Sensing, awareness, detection
    Reaction = 41,            // Quick decisions, responses
    Navigation = 42,          // Pathfinding, movement planning
    Evaluation = 43,          // Assessment, scoring

    // =========================================================================
    // Adaptive (5Hz) - AI planning and coordination
    // =========================================================================
    Deliberation = 50,        // Planning, reasoning
    Aggregation = 51,         // Data collection, rollups
    Correlation = 52,         // Pattern matching, relationships
    Coordination = 53,        // Multi-agent coordination

    // =========================================================================
    // Progressive (2Hz) - Slow adjustments
    // =========================================================================
    Modulation = 60,          // Intensity adjustments
    Regulation = 61,          // Homeostasis, balance
    Adaptation = 62,          // Learning, adjustment

    // =========================================================================
    // Cyclic (1Hz) - Regular intervals
    // =========================================================================
    Dissemination = 70,       // Hub publish, broadcasting
    Preservation = 71,        // Database write, persistence
    Observation = 72,         // Monitoring, watching

    // =========================================================================
    // Gradual (10s) - Slow accumulation
    // =========================================================================
    Accumulation = 80,        // Statistics gathering
    Consolidation = 81,       // Data merging

    // =========================================================================
    // Incremental (1min) - Maintenance tasks
    // =========================================================================
    Maintenance = 90,         // GC, cleanup
    Reconciliation = 91,      // Consistency checks

    // =========================================================================
    // Ambient (5min) - Very slow processes
    // =========================================================================
    Maturation = 100,         // Slow growth
    Degradation = 101,        // Slow decay

    // =========================================================================
    // Periodic (15min) - Periodic processes
    // =========================================================================
    Regeneration = 110,       // Recovery, healing
    Decomposition = 111,      // Breakdown

    // =========================================================================
    // Epochal (1h) - Hourly processes
    // =========================================================================
    Evolution = 120,          // Long-term change
    Erosion = 121,            // Geological processes

    // =========================================================================
    // Extended (6h) - Quarter-day processes
    // =========================================================================
    Succession = 130,         // Progression
    Transformation = 131,     // Major change

    // =========================================================================
    // Diurnal (24h real-time) - Daily processes
    // =========================================================================
    Culmination = 140,        // Daily peak
    Renewal = 141,            // Daily reset

    // =========================================================================
    // =========================================================================
    // GAME-TIME SCHEDULES (Antarian Calendar - Lexicon Temporis)
    // =========================================================================
    // These schedules are triggered by in-game time, NOT real-time.
    // See ARCH_ASE_CALENDAR.md for complete nomenclature and calculations.
    //
    // Time Units (Antarian):
    //   TUMBRAE (Day)       = 30 hours
    //   NOVENDRIX (Week)    = 9 TUMBRAE  (from "Neun-Tages-Interkalation")
    //   LUNBREX (Month)     = 27 TUMBRAE (Scorpii cycle)
    //   VECTUM MORTIS (Year)= 4021 TUMBRAE (Prime!)
    //
    // Moon Orbital Periods:
    //   FRACTUM AURIX  (Aurora-B) = 9 TUMBRAE
    //   MATRUM AURIX   (Aurora-A) = 13 TUMBRAE
    //   AMILOX SILENTUM (Amilo)   = 19 TUMBRAE
    //   SCORPIX OCCULTUM (Scorpii)= 27 TUMBRAE
    // =========================================================================
    // =========================================================================

    // =========================================================================
    // Diurnal Game-Time (1 TUMBRAE = 30 game-hours = 20 min real-time)
    // =========================================================================
    GameDawn = 150,           // VELUXIS - Game day starts (dawn)
    GameDusk = 151,           // CREPEX - Game day ends (dusk)
    GameMidnight = 152,       // NOXUMBRAE - Midnight (Serpens trigger!)

    // =========================================================================
    // Novendrix (1 NOVENDRIX = 9 TUMBRAE = 3h real-time)
    // =========================================================================
    Novendrix = 160,          // Weekly game events (9-day week)
    Serpum = 161,             // Day 6 of week - Serpens day (DANGEROUS!)
    Fractum = 162,            // Day 8 of week - Day of the Broken

    // =========================================================================
    // Lunbrex (1 LUNBREX = 27 TUMBRAE = 9h real-time)
    // =========================================================================
    Lunbrex = 170,            // Monthly game events (27-day month)
    ScorpiiCycle = 171,       // Scorpii moon full cycle complete

    // =========================================================================
    // Moon Conjunctions (CONIUNCTRIX) - Two-Moon Events
    // =========================================================================
    CrucixAurix = 175,        // Aurora Crossing (117 TUMBRAE = 39h)
    LunixSanguex = 176,       // Bloodmoon (513 TUMBRAE = 7.1 days)

    // =========================================================================
    // Temporix (Seasons - ~1000 TUMBRAE each, ~2 weeks real-time)
    // =========================================================================
    Temporix = 180,           // Season change events
    GlacixUmbrae = 181,       // Frostschatten starts (997 TUMBRAE)
    IgnixVertum = 182,        // Glutwende starts (1009 TUMBRAE)
    HalitrixNebulox = 183,    // Nebelhauch starts (1013 TUMBRAE)
    SaltatrixMortum = 184,    // Schattentanz starts - SERPENS ACTIVE! (1002 TUMBRAE)

    // =========================================================================
    // Stellar Cycles
    // =========================================================================
    PulsatrixAntarix = 185,   // Antares pulsation phase change (1087 TUMBRAE = 15 days)
    CyclumArcturix = 186,     // Arcturus visibility change (2011 TUMBRAE = 28 days)

    // =========================================================================
    // Rare Multi-Moon Conjunctions
    // =========================================================================
    TrinitaxLuminex = 187,    // Three-Moon conjunction (2223 TUMBRAE = 31 days)

    // =========================================================================
    // Vectum Mortis (1 VECTUM = 4021 TUMBRAE = 56 days real-time)
    // =========================================================================
    VectumMortis = 190,       // Yearly game events
    NewVectum = 191,          // Year change celebration

    // =========================================================================
    // Extreme Rare Events
    // =========================================================================
    TenebraxMagnorum = 195,   // The Great Darkness - All 4 moons align!
                              // (6669 TUMBRAE = 93 days real-time)

    // =========================================================================
    // Epochal Game-Time (AETRIX)
    // =========================================================================
    AetrixMinorum = 200,      // Small epoch (1 Arcturus = 4022 TUMBRAE)
    AetrixMaiorum = 201,      // Great epoch (10 Arcturus = 40220 TUMBRAE)

    // =========================================================================
    // Aevrix (100 Arcturus cycles = ~15 years real-time)
    // =========================================================================
    Aevrix = 210,             // Aeon events (402200 TUMBRAE)
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get the frequency tier name for a schedule
 * @param s The schedule to query
 * @return Tier name as string (e.g., "Lifecycle", "Frame", "GameTumbrae")
 */
[[nodiscard]] constexpr const char* schedule_tier(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    // Real-Time schedules
    if (val <= 3) return "Lifecycle";
    if (val >= 10 && val <= 14) return "Frame";
    if (val >= 20 && val <= 22) return "Kinetic";
    if (val >= 30 && val <= 31) return "Reactive";
    if (val >= 40 && val <= 43) return "Tactical";
    if (val >= 50 && val <= 53) return "Adaptive";
    if (val >= 60 && val <= 62) return "Progressive";
    if (val >= 70 && val <= 72) return "Cyclic";
    if (val >= 80 && val <= 81) return "Gradual";
    if (val >= 90 && val <= 91) return "Incremental";
    if (val >= 100 && val <= 101) return "Ambient";
    if (val >= 110 && val <= 111) return "Periodic";
    if (val >= 120 && val <= 121) return "Epochal";
    if (val >= 130 && val <= 131) return "Extended";
    if (val >= 140 && val <= 141) return "Diurnal";
    // Game-Time schedules (Antarian Calendar - Lexicon Temporis)
    if (val >= 150 && val <= 152) return "GameTumbrae";
    if (val >= 160 && val <= 162) return "GameNovendrix";
    if (val >= 170 && val <= 171) return "GameLunbrex";
    if (val >= 175 && val <= 176) return "GameConiunctrix";
    if (val >= 180 && val <= 184) return "GameTemporix";
    if (val >= 185 && val <= 187) return "GameStellar";
    if (val >= 190 && val <= 191) return "GameVectum";
    if (val == 195) return "GameTenebrax";
    if (val >= 200 && val <= 201) return "GameAetrix";
    if (val == 210) return "GameAevrix";
    return "Unknown";
}

/**
 * @brief Get the frequency in Hz for a schedule
 * @param s The schedule to query
 * @return Frequency in Hz (0 = once, -1 = unknown, -2 = game-time event)
 *
 * Game-Time schedules return -2 as they are event-driven, not time-driven.
 */
[[nodiscard]] constexpr float schedule_hz(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    // Real-Time schedules
    if (val <= 3) return 0.0f;                    // Once
    if (val >= 10 && val <= 14) return 60.0f;     // ~60Hz
    if (val >= 20 && val <= 22) return 30.0f;     // 30Hz
    if (val >= 30 && val <= 31) return 20.0f;     // 20Hz
    if (val >= 40 && val <= 43) return 10.0f;     // 10Hz
    if (val >= 50 && val <= 53) return 5.0f;      // 5Hz
    if (val >= 60 && val <= 62) return 2.0f;      // 2Hz
    if (val >= 70 && val <= 72) return 1.0f;      // 1Hz
    if (val >= 80 && val <= 81) return 0.1f;      // 0.1Hz (10s)
    if (val >= 90 && val <= 91) return 0.017f;    // 1/min
    if (val >= 100 && val <= 101) return 0.003f;  // 1/5min
    if (val >= 110 && val <= 111) return 0.001f;  // 1/15min
    if (val >= 120 && val <= 121) return 0.0003f; // 1/hour
    if (val >= 130 && val <= 131) return 0.00005f;// 1/6hours
    if (val >= 140 && val <= 141) return 0.00001f;// 1/day
    // Game-Time schedules (event-driven, not Hz-based)
    if (val >= 150) return -2.0f;                 // Game-Time event-driven
    return -1.0f;
}

/**
 * @brief Get the interval in seconds for a schedule
 * @param s The schedule to query
 * @return Interval in seconds (0 = once/frame, -1 = unknown, -2 = game-time event)
 *
 * Game-Time schedules return -2 as they are event-driven based on in-game calendar.
 */
[[nodiscard]] constexpr float schedule_interval(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    // Real-Time schedules
    if (val <= 3) return 0.0f;                    // Once
    if (val >= 10 && val <= 14) return 0.016f;    // ~16ms
    // BERECHNET, NICHT GERUNDET — und das ist der einzige Tier, bei dem der Unterschied
    // die VERGLEICHSRICHTUNG dreht. Der Taktgeber feuert mit `accumulator >= interval`
    // (internal/tick_scheduler.cpp:191/200) gegen `TIER_CONFIGS[Kinetic].interval`, und das
    // ist dort `1.0f/30.0f` (:129). Gegen das gerundete `0.033f` gilt:
    //     0.032999999821186066  <  0.03333333507180214   →  `>=` FALSCH, der Tier bleibt STUMM
    // Wer mit `schedule_interval(Schedule::Dynamics)` tickte, liess den Physik-Tier also
    // nicht laufen und prueft danach eine Registry, die niemand angefasst hat. Alle uebrigen
    // Tiers sind bitgleich (20Hz/10Hz/5Hz/2Hz), deshalb faellt es nur hier auf.
    if (val >= 20 && val <= 22) return 1.0f / 30.0f;  // 30Hz — Kinetic, siehe oben
    if (val >= 30 && val <= 31) return 0.05f;     // 50ms
    if (val >= 40 && val <= 43) return 0.1f;      // 100ms
    if (val >= 50 && val <= 53) return 0.2f;      // 200ms
    if (val >= 60 && val <= 62) return 0.5f;      // 500ms
    if (val >= 70 && val <= 72) return 1.0f;      // 1s
    if (val >= 80 && val <= 81) return 10.0f;     // 10s
    if (val >= 90 && val <= 91) return 60.0f;     // 1min
    if (val >= 100 && val <= 101) return 300.0f;  // 5min
    if (val >= 110 && val <= 111) return 900.0f;  // 15min
    if (val >= 120 && val <= 121) return 3600.0f; // 1hour
    if (val >= 130 && val <= 131) return 21600.0f;// 6hours
    if (val >= 140 && val <= 141) return 86400.0f;// 24hours
    // Game-Time schedules (event-driven, not interval-based)
    if (val >= 150) return -2.0f;                 // Game-Time event-driven
    return -1.0f;
}

/**
 * @brief Check if a schedule is a lifecycle schedule (runs once)
 * @param s The schedule to check
 * @return true if lifecycle schedule (Initialization, Configuration, Termination, Finalization)
 */
[[nodiscard]] constexpr bool is_lifecycle_schedule(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    return val <= 3;
}

/**
 * @brief Check if a schedule is a frame schedule (~60Hz)
 * @param s The schedule to check
 * @return true if frame schedule (Reception, Ingestion, Integration, Production, Conclusion)
 */
[[nodiscard]] constexpr bool is_frame_schedule(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    return val >= 10 && val <= 14;
}

/**
 * @brief Check if a schedule is a fixed-timestep schedule (30Hz or slower)
 * @param s The schedule to check
 * @return true if fixed-timestep (Kinetic and slower)
 */
[[nodiscard]] constexpr bool is_fixed_schedule(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    return val >= 20;
}

/**
 * @brief Check if a schedule is a game-time schedule (event-driven by Antarian calendar)
 * @param s The schedule to check
 * @return true if game-time schedule (GameDawn and later)
 */
[[nodiscard]] constexpr bool is_gametime_schedule(Schedule s) noexcept {
    uint8_t val = static_cast<uint8_t>(s);
    return val >= 150;
}

/**
 * One row of the schedule name table: the value and the text it prints as.
 *
 * WHY A TABLE AND NOT A SWITCH: a switch over these 66 values is a type dispatch written as
 * control flow, and the rule against it is not cosmetic — every added schedule needs a new
 * branch in a function that already spans a hundred lines, and a forgotten one falls silently
 * into the default. As data, a schedule is ONE line, and a missing line is visible as a
 * missing line.
 *
 * WHY PAIRS AND NOT AN INDEXED ARRAY: the enum is deliberately sparse — tiers occupy their own
 * decade, so 66 schedules span the range 0..210. MEASURED: an array indexed by the enum value
 * would need 211 slots of which 145 are holes, and every future insertion would have to land
 * on exactly the right index. The pair form has no holes and no position to get wrong.
 */
struct ScheduleNameRow {
    Schedule    value;
    const char* name;
};

/**
 * @brief Schedule names, in tier order (for logging).
 *
 * The comments below are the tiers, kept from the switch this replaced — they are the reason
 * the order is not alphabetical and must survive any future edit.
 */
inline constexpr ScheduleNameRow kScheduleNames[] = {
    // Lifecycle
    {Schedule::Initialization,   "Initialization"},
    {Schedule::Configuration,    "Configuration"},
    {Schedule::Termination,      "Termination"},
    {Schedule::Finalization,     "Finalization"},
    // Frame
    {Schedule::Reception,        "Reception"},
    {Schedule::Ingestion,        "Ingestion"},
    {Schedule::Integration,      "Integration"},
    {Schedule::Production,       "Production"},
    {Schedule::Conclusion,       "Conclusion"},
    // Kinetic
    {Schedule::Dynamics,         "Dynamics"},
    {Schedule::Kinematics,       "Kinematics"},
    {Schedule::Collision,        "Collision"},
    // Reactive
    {Schedule::Transmission,     "Transmission"},
    {Schedule::Synchronization,  "Synchronization"},
    // Tactical
    {Schedule::Perception,       "Perception"},
    {Schedule::Reaction,         "Reaction"},
    {Schedule::Navigation,       "Navigation"},
    {Schedule::Evaluation,       "Evaluation"},
    // Adaptive
    {Schedule::Deliberation,     "Deliberation"},
    {Schedule::Aggregation,      "Aggregation"},
    {Schedule::Correlation,      "Correlation"},
    {Schedule::Coordination,     "Coordination"},
    // Progressive
    {Schedule::Modulation,       "Modulation"},
    {Schedule::Regulation,       "Regulation"},
    {Schedule::Adaptation,       "Adaptation"},
    // Cyclic
    {Schedule::Dissemination,    "Dissemination"},
    {Schedule::Preservation,     "Preservation"},
    {Schedule::Observation,      "Observation"},
    // Gradual
    {Schedule::Accumulation,     "Accumulation"},
    {Schedule::Consolidation,    "Consolidation"},
    // Incremental
    {Schedule::Maintenance,      "Maintenance"},
    {Schedule::Reconciliation,   "Reconciliation"},
    // Ambient
    {Schedule::Maturation,       "Maturation"},
    {Schedule::Degradation,      "Degradation"},
    // Periodic
    {Schedule::Regeneration,     "Regeneration"},
    {Schedule::Decomposition,    "Decomposition"},
    // Epochal
    {Schedule::Evolution,        "Evolution"},
    {Schedule::Erosion,          "Erosion"},
    // Extended
    {Schedule::Succession,       "Succession"},
    {Schedule::Transformation,   "Transformation"},
    // Diurnal (Real-Time)
    {Schedule::Culmination,      "Culmination"},
    {Schedule::Renewal,          "Renewal"},

    // =========================================================================
    // GAME-TIME SCHEDULES (Antarian Calendar - Lexicon Temporis)
    // =========================================================================
    // GameTumbrae (1 TUMBRAE = 30 hours = 20 min real-time)
    {Schedule::GameDawn,         "GameDawn"},
    {Schedule::GameDusk,         "GameDusk"},
    {Schedule::GameMidnight,     "GameMidnight"},
    // GameNovendrix (9 TUMBRAE = 3h real-time)
    {Schedule::Novendrix,        "Novendrix"},
    {Schedule::Serpum,           "Serpum"},
    {Schedule::Fractum,          "Fractum"},
    // GameLunbrex (27 TUMBRAE = 9h real-time)
    {Schedule::Lunbrex,          "Lunbrex"},
    {Schedule::ScorpiiCycle,     "ScorpiiCycle"},
    // GameConiunctrix (Moon Conjunctions)
    {Schedule::CrucixAurix,      "CrucixAurix"},
    {Schedule::LunixSanguex,     "LunixSanguex"},
    // GameTemporix (Seasons ~2 weeks real-time)
    {Schedule::Temporix,         "Temporix"},
    {Schedule::GlacixUmbrae,     "GlacixUmbrae"},
    {Schedule::IgnixVertum,      "IgnixVertum"},
    {Schedule::HalitrixNebulox,  "HalitrixNebulox"},
    {Schedule::SaltatrixMortum,  "SaltatrixMortum"},
    // GameStellar (Stellar cycles)
    {Schedule::PulsatrixAntarix, "PulsatrixAntarix"},
    {Schedule::CyclumArcturix,   "CyclumArcturix"},
    {Schedule::TrinitaxLuminex,  "TrinitaxLuminex"},
    // GameVectum (1 VECTUM = 4021 TUMBRAE = 56 days)
    {Schedule::VectumMortis,     "VectumMortis"},
    {Schedule::NewVectum,        "NewVectum"},
    // GameTenebrax (The Great Darkness)
    {Schedule::TenebraxMagnorum, "TenebraxMagnorum"},
    // GameAetrix (Epochs)
    {Schedule::AetrixMinorum,    "AetrixMinorum"},
    {Schedule::AetrixMaiorum,    "AetrixMaiorum"},
    // GameAevrix (Aeon)
    {Schedule::Aevrix,           "Aevrix"},
};

/** Number of named schedules. Bound to the table, so it can never drift from it. */
inline constexpr uint32_t kScheduleNameCount =
    static_cast<uint32_t>(sizeof(kScheduleNames) / sizeof(kScheduleNames[0]));

// The count is a fact about the tier structure, not a free number: 42 real-time schedules
// across 15 tiers plus 24 game-time schedules across 10. If this assert fires, a row was added
// or lost, and the tier comments above say which block it belongs to.
static_assert(kScheduleNameCount == 66, "schedule name table lost or gained a row");

/**
 * @brief Get schedule name as text (for logging).
 * @param s The schedule to query
 * @return the name, or "Unknown" for a value that carries no row
 *
 * Returns const char*, not a string_view: this is Layer 1, and the callers append the result
 * to a std::string or hand it to a log format — both take a bare pointer without a detour.
 *
 * The scan is linear over 66 rows and that is deliberate. All four call sites are diagnostic
 * (boot table, dependency-cycle error) and none of them runs inside a tick; in a constant
 * expression the loop costs nothing at all, because it never reaches runtime.
 */
[[nodiscard]] constexpr const char* schedule_name(Schedule s) noexcept {
    for (const ScheduleNameRow& row : kScheduleNames) {
        if (row.value == s) {
            return row.name;
        }
    }
    return "Unknown";
}

}  // namespace ase::ecs

// Hier stand eine Spezialisierung von std::hash fuer Schedule. Ihr eigener Kommentar nannte
// ihren Zweck — "required for unordered_map" — und damit auch den Grund, warum sie weg ist:
// den Behaeltertyp, fuer den sie gebraucht wurde, gibt es in diesem Baum nicht mehr, seit
// ase-containers ihn ersetzt hat.
//
// GEMESSEN vor dem Loeschen: baumweit kein einziger Treffer auf unordered_map<Schedule>,
// unordered_set<Schedule> oder hash<Schedule> ausser der Spezialisierung selbst. Sie hat
// keinen Nutzer und kann keinen bekommen, ohne dass jemand zuerst den verbotenen Behaelter
// zurueckholt.
