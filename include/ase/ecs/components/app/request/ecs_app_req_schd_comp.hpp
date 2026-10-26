#pragma once

/**
 * ASE ECS COMPONENT
 *
 * @file        ecs_app_req_schd_comp.hpp
 * @brief       EcsAppReqSchdComponent - request to fire one game-time schedule
 * @description One entity per crossed boundary of the Antarian calendar. The module that computes
 *              the date (ase-calendar, L3) creates it when the game time crosses the start of a
 *              TUMBRAE, a NOVENDRIX, a season or any other game-time event; the App drains every
 *              request once per tick, after the real-time tiers and before Conclusion, runs the
 *              named schedule with the game seconds of its period as dt, and destroys the request
 *              (ARCH_ASE_SCHEDULE section 6.3).
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    communication/request/requesttype
 * @usecase     usecase:100357 - run the tick loop
 * @actor       Entity Component System (ECS)
 * @parity      server_only
 * @created     2026-10-25
 * @modified    2026-10-25
 * @version     1.0.0
 *
 * WHY THE TYPE STANDS IN L1: the scheduler (L1) must never read the calendar (L3), and the
 * calendar must never call the App. The request is the seam: L1 owns the type and knows nothing
 * of dates, L3 writes it and depends downward only. The device runs the same C++ App embedded,
 * so its own calendar creates the same requests; the type is declared here and nowhere else.
 *
 * ECS COMPONENT COMPLIANCE
 *
 * [ ] DATA fields ONLY - No methods
 * [ ] NO .cpp file - Header-only
 * [ ] ONLY zero-initialization (= 0, = 0.0f, = false, = {})
 * [ ] No magic numbers in defaults (use types.hpp constants)
 * [ ] Entity references initialized to = 0 (systems set values)
 * [ ] Single responsibility (one data category)
 * [ ] No God-Component (unrelated fields)
 * [ ] Large data in registry.ctx()? (component has only lookup ID!)
 * [ ] Tag structs end with Tag suffix - N/A (not a tag)
 * [ ] Filename: prefix/suffix NOT abbreviated, words between = 3-4 chars
 * [ ] Struct name derived from filename (snake_case to PascalCase)
 * [ ] 1 File = 1 Component
 * [ ] File in correct category subfolder
 * [ ] SHARED components listed in codegen.json components.shared
 * [ ] Pointer components in codegen.json components.server_only
 * [ ] Strings < 64 bytes use char[N] fixed arrays
 * [ ] Strings 64-256 bytes use appropriately sized char[N]
 * [ ] Strings > 256 bytes use registry.ctx() with lookup ID?
 * [ ] NO Entity-per-Character (strings are single attributes, not N-Items!)
 * [ ] Lookup-only strings use uint32_t hash (entt::hashed_string)
 * [ ] NO std::shared_ptr in components (use Flyweight Pattern via ctx!)
 * [ ] NO void* in components (use Flyweight Pattern via ctx!)
 * [ ] NO uint64_t as pointer concept (use uint32_t ID + ResourceManager via ctx!)
 * [ ] External library objects (shared_ptr, handles) in ResourceManager via ctx()
 * [ ] Component stores ONLY primitive ID (uint32_t) referencing external resource
 */

#include <cstdint>

namespace ase::ecs {

/**
 * EcsAppReqSchdComponent - one crossed game-time boundary, waiting to fire its schedule
 *
 * schedule is the numeric value of a game-time Schedule (GameDawn = 150 to Aevrix = 210); a
 * value outside that range is refused by the App with an error line, because a request for a
 * real-time schedule would run it a second time in the same tick.
 *
 * sequence orders the requests of one tick: when a long span is caught up, several boundaries
 * fall into one tick, and they fire in the order the calendar crossed them, lowest first.
 */
struct EcsAppReqSchdComponent {
    uint32_t schedule = 0;  // numeric value of the game-time Schedule to fire (150 to 210)
    uint32_t sequence = 0;  // order of the crossing within the tick, lowest fires first
    float game_dt = 0.0f;   // game seconds of the period this run stands for, handed in as dt
};

}  // namespace ase::ecs
