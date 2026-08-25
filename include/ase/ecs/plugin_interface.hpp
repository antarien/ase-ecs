#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        plugin_interface.hpp
 * @brief       C ABI between the kernel and every dlopen'd module and plugin
 * @description The single symbol a shared object exports (`ase_module_interface`
 *              or `ase_plugin_interface`) and the function-pointer table behind
 *              it. The kernel's ModuleLoader finds it with dlsym at runtime, so
 *              this file is the one place in the tree where the layout is fixed
 *              by the loader rather than by the compiler.
 *
 *              TWO RULE CONFLICTS STAND HERE ON PURPOSE, measured 2026-08-20:
 *
 *                extern "C"   the exported symbol must not be mangled - dlsym
 *                             looks it up by that exact name, and a C++
 *                             interface is precisely what it cannot find.
 *                Makros       the registration macro writes a designated
 *                             initialiser list for the C struct; a constexpr
 *                             cannot generate the symbol definition a loader
 *                             has to see.
 *
 *              The rules that name these are written for engine runtime code.
 *              Here they name replacements that would remove the loading
 *              mechanism itself.
 *
 *              THE ase-markdown COMPARISON THAT STOOD HERE IS WITHDRAWN,
 *              measured 2026-08-22. It read "the same class as the WASM
 *              boundary in ase-markdown" - and that boundary did NOT keep its
 *              C linkage. src/wasm/markdown_wasm.cpp says so itself: three
 *              findings named the construction, "all three were right", and
 *              embind replaced it. The module now stands at 0 violations over
 *              13 files WITH that file checked as core_impl, and it contains
 *              extern "C" exactly zero times.
 *
 *              THE DIFFERENCE IS THE ONE THAT MATTERS, and it is why this file
 *              still cannot follow: embind IS the C++ interface the rule asks
 *              for, offered by the library itself. POSIX dlsym offers no such
 *              thing - it resolves a symbol by its exact unmangled name, and
 *              there is no C++ interface that produces one. The WASM case had
 *              a replacement available and, measured at the time, no caller at
 *              all; this one has neither: 125 files in the tree depend on these
 *              types, 119 through the export macros.
 *
 *              So the conflict stands, but it stands on its own measurement -
 *              not on a neighbour that has since moved.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-01-16
 * @modified    2026-08-20
 * @version     1.1.0
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
 *
 * C ABI Interface for dynamically loaded Modules (L3) and Plugins (L4).
 *
 * Modules/Plugins export a single symbol (`ase_module_interface` or
 * `ase_plugin_interface`) containing function pointers for lifecycle
 * management. The Kernel's ModuleLoader discovers and loads these at
 * runtime via dlopen/dlsym.
 *
 * Usage in a Module (L3):
 *
 *   #include <ase/ecs/plugin_interface.hpp>
 *   #include <ase/hub/hub_module.hpp>
 *
 *   static AseModuleInfo get_info() {
 *       return { "ase-hub", "0.19.25", ASE_API_VERSION, 3 };
 *   }
 *
 *   static uint32_t on_load(AseLoadContext* ctx) {
 *       ase::hub::HubModule instance;
 *       instance.build(*ctx->app);
 *       return ASE_LOAD_OK;
 *   }
 *
 *   static uint32_t on_unload(AseLoadContext*) { return ASE_LOAD_OK; }
 *   static void cleanup() {}
 *
 *   ASE_MODULE_EXPORT(get_info, on_load, on_unload, cleanup)
 *
 * Usage in a Plugin (L4):
 *
 *   #include <ase/ecs/plugin_interface.hpp>
 *   #include <ase/pl-sky/sky_plugin.hpp>
 *
 *   static AseModuleInfo get_info() {
 *       return { "ase-pl-sky", "0.1.7", ASE_API_VERSION, 4 };
 *   }
 *
 *   static uint32_t on_load(AseLoadContext* ctx) {
 *       ase::sky::SkyPlugin instance;
 *       instance.build(*ctx->app);
 *       return ASE_LOAD_OK;
 *   }
 *
 *   static uint32_t on_unload(AseLoadContext*) { return ASE_LOAD_OK; }
 *   static void cleanup() {}
 *
 *   ASE_PLUGIN_EXPORT(get_info, on_load, on_unload, cleanup)
 *
 * References:
 *   ARCH_ASE_PLUGIN.md - Plugin Development Guide
 *   WRFL_ASE_MODULE_DEPENDENCIES.md - Layer Isolation Rules
 */

#include <cstdint>
#include <ase/ecs/system.hpp>  // Registry is a using-alias, cannot be forward-declared

// Vorwaertsdeklarationen in der MEHRZEILIGEN Hausform (so wie die Nachbarheader in ase-ecs).
// Die einzeilige Fassung trug denselben Abschlusskommentar, aber nicht am Zeilenanfang — und
// genau darauf sieht die Lint-Regel (`^}  // namespace ase::`). Gleiche Bedeutung, gleiche ABI,
// nur die Form, die der Baum ueberall sonst schon hat.
namespace ase::ecs {
class App;
}  // namespace ase::ecs

namespace ase::kernel {
class KernelServiceRegistry;
class KernelEventBus;
class KernelConfigRegistry;
}  // namespace ase::kernel

// =============================================================================
// API Version (Exact match required between loader and loaded module)
// =============================================================================

inline constexpr uint32_t ASE_API_VERSION = 4;

// =============================================================================
// Load Result Codes (returned by on_load / on_unload)
// =============================================================================

inline constexpr uint32_t ASE_LOAD_OK             = 0;
inline constexpr uint32_t ASE_LOAD_ERROR          = 1;
inline constexpr uint32_t ASE_LOAD_MISSING_DEP    = 2;
inline constexpr uint32_t ASE_LOAD_MISSING_CONFIG = 3;
inline constexpr uint32_t ASE_LOAD_INIT_FAILED    = 4;

// =============================================================================
// Module Info (returned by get_info callback)
// =============================================================================

struct AseModuleInfo {
    const char* name;           // e.g., "ase-hub", "ase-pl-sky"
    const char* version;        // e.g., "0.19.25" (semver from VERSION file)
    uint32_t    api_version;    // Must match ASE_API_VERSION
    uint32_t    layer;          // 3 = Module (L3), 4 = Plugin (L4)
};

// =============================================================================
// Plugin Context (passed to on_load/on_unload)
// =============================================================================

struct AseLoadContext {
    ase::ecs::App*                       app       = nullptr;
    ase::ecs::Registry*                  registry  = nullptr;
    ase::kernel::KernelServiceRegistry*  services  = nullptr;
    ase::kernel::KernelEventBus*         events    = nullptr;
    ase::kernel::KernelConfigRegistry*   config    = nullptr;
};

// =============================================================================
// Function pointer types
// =============================================================================

using AseModuleInfoFn          = AseModuleInfo (*)();
using AseModuleLoadFn          = uint32_t (*)(AseLoadContext*);
using AseModuleUnloadFn        = uint32_t (*)(AseLoadContext*);
using AseModuleConfigChangedFn = void (*)(AseLoadContext*, const char* key, const char* value);
using AseModuleCleanFn         = void (*)();

// =============================================================================
// Module Interface (exported as C symbol via dlsym)
// =============================================================================

struct AseModuleInterface {
    AseModuleInfoFn          get_info;          // Returns module metadata
    AseModuleLoadFn          on_load;           // Registers systems (receives full context)
    AseModuleUnloadFn        on_unload;         // Called before hot-reload or shutdown
    AseModuleConfigChangedFn on_config_changed; // Called when TOML config key changes at runtime
    AseModuleCleanFn         cleanup;           // Called before dlclose (resource cleanup)
};

// =============================================================================
// Export Macros
// =============================================================================

// L3 Module: exports symbol "ase_module_interface"
#define ASE_MODULE_EXPORT(info_fn, load_fn, unload_fn, cfg_fn, cleanup_fn) \
    extern "C" {                                                          \
        AseModuleInterface ase_module_interface = {                        \
            /* .get_info          = */ info_fn,                            \
            /* .on_load           = */ load_fn,                            \
            /* .on_unload         = */ unload_fn,                          \
            /* .on_config_changed = */ cfg_fn,                             \
            /* .cleanup           = */ cleanup_fn                          \
        };                                                                \
    }

// L4 Plugin: exports symbol "ase_plugin_interface"
#define ASE_PLUGIN_EXPORT(info_fn, load_fn, unload_fn, cfg_fn, cleanup_fn) \
    extern "C" {                                                          \
        AseModuleInterface ase_plugin_interface = {                        \
            /* .get_info          = */ info_fn,                            \
            /* .on_load           = */ load_fn,                            \
            /* .on_unload         = */ unload_fn,                          \
            /* .on_config_changed = */ cfg_fn,                             \
            /* .cleanup           = */ cleanup_fn                          \
        };                                                                \
    }
