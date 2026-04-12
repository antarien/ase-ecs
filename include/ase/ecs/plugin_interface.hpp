#pragma once

/**
 * @file        plugin_interface.hpp
 * @module      ase-ecs
 * @layer       1 (Core)
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

namespace ase::ecs { class App; }
namespace ase::kernel { class KernelServiceRegistry; class KernelEventBus; class KernelConfigRegistry; }

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
