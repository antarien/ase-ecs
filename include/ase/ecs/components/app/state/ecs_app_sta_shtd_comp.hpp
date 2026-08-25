#pragma once

/**
 * ASE ECS COMPONENT
 *
 * @file        ecs_app_sta_shtd_comp.hpp
 * @brief       Descriptor of the armed shutdown watch (singleton)
 * @description Carries the signalfd descriptor over which SIGINT/SIGTERM/SIGHUP are delivered
 *              to the running process. App arms the watch in startup(), reads it once per
 *              tick() and closes it in shutdown().
 *
 *              DIE ANWESENHEIT DIESES COMPONENTS IST DIE AUSSAGE, nicht sein Wert. Kein
 *              Component heisst „keine Wache scharf" — deshalb braucht das Feld keinen
 *              Ungueltigkeitswert und die Zero-Init-Regel bleibt unverletzt. Ein Deskriptor 0
 *              waere sonst nicht von „nicht eingerichtet" zu unterscheiden, denn 0 ist ein
 *              gueltiger Deskriptor (stdin).
 *
 *              WARUM ES DIESES COMPONENT GIBT: hier stand bis zum 2026-08-22 ein Signalhandler
 *              mit einem globalen App*-Zeiger in app.cpp. Zwei Regeln zeigten unabhaengig
 *              voneinander auf dieselbe Bauform — SIGNAL_HANDLERS_FORBIDDEN („Use ECS event
 *              pattern") und GLOBAL_VARIABLE_FORBIDDEN („Use Components or registry.ctx()").
 *              Das Signal ist jetzt ein EREIGNIS, das der Tick abholt, und sein Deskriptor
 *              liegt dort, wo ECS-Zustand liegt.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    state/lifecycle/initialization
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
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
 * [ ] Strings > 256 bytes use registry.ctx() mit Lookup-ID?
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
 * EcsAppStaShtdComponent - Descriptor of the armed shutdown watch (Singleton)
 *
 * App writes this once in startup() and reads it once per tick(); nothing above L1 touches it.
 * The descriptor is a plain kernel handle (int32_t), not a pointer and not an owning object —
 * the Flyweight rule about shared_ptr and void* does not apply, and no ResourceManager is
 * needed for a single integer whose owner is the App itself.
 *
 * KEIN SERVER-ONLY/SHARED-EINTRAG: ase-ecs ist L1 und traegt keine codegen.json. Ein
 * Prozesssignal hat auf dem Client ohnehin keine Entsprechung — der Browser kennt kein SIGTERM.
 */
struct EcsAppStaShtdComponent {
    int32_t watch_fd = 0;  // signalfd descriptor; PRESENCE of this component means "watch armed"
};

}  // namespace ase::ecs
