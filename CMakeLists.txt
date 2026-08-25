cmake_minimum_required(VERSION 3.20)

#[[
ASE CORE BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-ecs
@description ECS framework - EnTT wrapper with App, System, Registry.
             Core Entity-Component-System architecture.

@module      ase-ecs
@layer       1 (Core)
@created     2025-12-15
@modified    2026-01-30
@version     00.00.32.00032 [core]

CMAKE MODULE COMPLIANCE

[ ] cmake_minimum_required(VERSION 3.20) as FIRST line
[ ] project() with VERSION and LANGUAGES CXX
[ ] message(STATUS) for configuration output
[ ] add_library() with STATIC and EXPLICIT file list
[ ] add_library(ase::{module} ALIAS ...) for namespace
[ ] target_include_directories() with PUBLIC and generator expressions
[ ] target_compile_features() with cxx_std_20
[ ] target_link_libraries() with PUBLIC/PRIVATE separation
[ ] if(ASE_BUILD_TESTS) block for tests
[ ] include(GNUInstallDirs) for installation
[ ] install(DIRECTORY include/ ...) for headers
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_ECS_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_ECS=(.*)$")
            set(ASE_ECS_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_ECS_VERSION)
        message(FATAL_ERROR "ASE_ECS version not found in VERSION file")
    endif()
endif()

project(ase-ecs VERSION ${ASE_ECS_VERSION} LANGUAGES CXX)

# Generate version.hpp from template (SSOT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ase/ecs/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/ase/ecs/version.hpp"
    @ONLY
)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================



message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

add_library(${PROJECT_NAME} STATIC
    #[[
    CORE
    ECS framework core: System base class, App runner.
    ]]
    src/app.cpp

    #[[
    INTERNAL
    Internal implementation details for App decomposition.
    ]]
    src/internal/terminal_utils.cpp
    src/internal/system_registry.cpp
    src/internal/dependency_sorter.cpp
    src/internal/tick_scheduler.cpp
    src/internal/boot_logger.cpp
    src/internal/shutdown_sequence.cpp

    #[[
    HEADERS - CORE
    Core ECS framework headers: App, System, Schedule, Types.
    ]]
    include/ase/ecs/app.hpp
    include/ase/ecs/system.hpp
    include/ase/ecs/schedule.hpp
    include/ase/ecs/plugin_interface.hpp

    #[[
    HEADERS - INTERNAL
    Internal implementation headers for App decomposition.
    ]]
    include/ase/ecs/internal/boot_logger.hpp
    include/ase/ecs/internal/dependency_sorter.hpp
    include/ase/ecs/internal/shutdown_sequence.hpp
    include/ase/ecs/internal/system_registry.hpp
    include/ase/ecs/internal/terminal_utils.hpp
    include/ase/ecs/internal/tick_scheduler.hpp

    #[[
    HEADERS - COMPONENTS - APP
    App state components for rates and timing, plus the quit-request mark that
    lets any layer above L1 end the run loop through the registry (the ECS form
    of App::quit(); App::tick() polls it, KernelCoreLfcSystem stamps it).
    ]]
    include/ase/ecs/components/app/state/ecs_app_sta_mod_tim_comp.hpp
    include/ase/ecs/components/app/state/ecs_app_sta_shtd_comp.hpp
    include/ase/ecs/components/app/state/ecs_app_sta_tim_comp.hpp
    include/ase/ecs/components/app/tag/ecs_app_quit_req_tag.hpp

    #[[
    HEADERS - COMPONENTS - TAG
    Die vier Lebenszyklus-Marken des ECS. system.hpp:565-579 fuehrt sie als
    geschlossenen Satz; die Definitionen liegen seit dem Herausloesen in je einem
    eigenen Header und gehoeren damit in diese Liste.
    KEIN RE-EXPORT-Block: ecs_dty_tag und ecs_pend_sync_tag haben Verbraucher
    (ase-sdk, ase-pl-sky), ecs_dstr_tag und ecs_crtd_tag haben baumweit KEINEN
    ausser ihrer eigenen Definition. Eine Fassaden-Zusage waere hier eine
    Behauptung ueber Konsumenten, die es nicht gibt — der offene Befund von
    validate_header_reachability bleibt deshalb sichtbar stehen, statt still
    umdeklariert zu werden.
    ]]
    include/ase/ecs/components/tag/ecs_crtd_tag.hpp
    include/ase/ecs/components/tag/ecs_dstr_tag.hpp
    include/ase/ecs/components/tag/ecs_dty_tag.hpp
    include/ase/ecs/components/tag/ecs_pend_sync_tag.hpp
)
add_library(ase::ecs ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        EnTT::EnTT       # External - Entity Component System
        ase::log         # Layer 1 - Logging
        ase::containers  # Layer 0 - SSOT Vector/HashMap/HashSet/Array (public headers, no std:: containers)
        ase::types       # Layer 0 - region_wire module-group registry (M-B attribution seam)
    PRIVATE
        # Layer 0, header-only: wall_time_millis() for the boot-line timestamps in
        # src/internal/terminal_utils.cpp, and monotonic_nanos() for the frame and per-system
        # timing in src/app.cpp.
        #
        # This block used to predict its own successor: "app.hpp still spells its frame timing
        # in std::chrono, and the day that moves to ase/utils/clock.hpp this entry has to
        # become PUBLIC with it." That day was 2026-08-20 — and the prediction turned out one
        # step too pessimistic. app.hpp no longer reads a clock AT ALL: its three public
        # aliases (Clock/Duration/TimePoint) are gone, nobody outside the module ever used
        # them (measured: 0 hits tree-wide), and the remaining member is a plain int64_t of
        # monotonic nanoseconds. The clock header is included by app.cpp alone, so PRIVATE is
        # still correct and stays.
        ase::utils
        # Layer 0: sleep_nanos() for the boot and shutdown pacing, which spelled its wait in
        # std::this_thread::sleep_for. Same condition as above - PRIVATE for as long as only
        # .cpp files reach for it.
        ase::platform
)
#[[ VERBOTEN: No Layer 3 dependencies! ]]

if(ASE_BUILD_TESTS)
    add_executable(${PROJECT_NAME}-test tests/test_ecs.cpp)
    target_link_libraries(${PROJECT_NAME}-test PRIVATE ase::ecs)
    target_compile_features(${PROJECT_NAME}-test PRIVATE cxx_std_20)
endif()

include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
