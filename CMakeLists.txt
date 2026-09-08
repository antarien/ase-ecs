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
@modified    2026-09-08
@version     00.01.11.00061 [feat]

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
    Core ECS framework headers: App, Entity, System, World, Schedule.
    entity.hpp und world.hpp sind am 2026-08-30 aus system.hpp herausgeschnitten worden -
    Vokabular, Systemvertrag und Weltfassade waren drei Zwecke in 625 Zeilen. system.hpp
    inkludiert beide weiter, damit keine der 1962 einbindenden Dateien eine Zeile aendern muss.
    Das Vokabular heisst NICHT types.hpp: dieser Name macht eine Datei zur Konstanten-SSOT ihres
    Moduls, und entity.hpp definiert keine einzige Konstante. Die Vorlage waere erfuellbar
    gewesen, die Zusage darin nicht wahr.
    ]]
    include/ase/ecs/app.hpp
    include/ase/ecs/entity.hpp
    include/ase/ecs/system.hpp
    include/ase/ecs/world.hpp
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
    App state components for rates and timing, plus die zwei Marken, mit denen eine Schicht
    ueber L1 auf den Ablauf einwirkt, ohne dass die App etwas ueber Tiers wuesste.
    ecs_app_quit_req_tag: die ECS-Form von App::quit() - App::tick() liest sie,
    KernelCoreLfcSystem stempelt sie.
    ecs_app_rgn_gate_tag: armiert das Regionstor in App::run_schedule; der WORLD-Kernel
    stempelt sie einmal bei der Tier-Initialisierung, auf jeder anderen Tier bleibt das Tor
    kalt und die Schleife byteweise wie zuvor.

    ecs_app_rgn_gate_tag STAND BIS 2026-08-31 IN KEINER CMakeLists, obwohl ZWEI Einheiten sie
    einbinden - core/ase-ecs/src/app.cpp:39 und kernel/ase-kernel/src/core/kernel_tier_ini_sys.cpp:146.
    Gefunden hat es validate_cmake_files, und zwar erst, als die Batterie zum ersten Mal fuer
    core/ase-ecs selbst gefahren wurde: die Einheit liegt in KEINEM Tier-Zuschnitt, also hat
    dieses Tor sie nie gesehen. Eine vergessene Eintragung, keine tote Datei - der Meldetext
    sagt es selbst, und die Loeschung waere hier der Verlust des Regionstors gewesen.
    ]]
    include/ase/ecs/components/app/state/ecs_app_sta_mod_tim_comp.hpp
    include/ase/ecs/components/app/state/ecs_app_sta_shtd_comp.hpp
    include/ase/ecs/components/app/state/ecs_app_sta_tim_comp.hpp
    include/ase/ecs/components/app/tag/ecs_app_quit_req_tag.hpp
    include/ase/ecs/components/app/tag/ecs_app_rgn_gate_tag.hpp

    #[[
    HEADERS - COMPONENTS - TAG
    Die vier Lebenszyklus-Marken des ECS. Der Grabsteinblock in system.hpp fuehrt sie als
    geschlossenen Satz; die Definitionen liegen seit dem Herausloesen in je einem
    eigenen Header und gehoeren damit in diese Liste.
    (Hier stand bis 2026-08-30 "system.hpp:565-579" - der Schnitt von system.hpp hat die
    Zeilennummer verschoben, und eine Zeilennummer in einer Beschreibung altert bei jedem
    Schrieb an der beschriebenen Datei, ohne dass ein Tor es meldet.)
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

#[[
DAS TESTZIEL STAND BIS 2026-09-01 HIER INLINE, und das war der Grund fuer einen offenen Posten,
nicht nur eine Formfrage. Die drei Zeilen lauteten:

    add_executable(${PROJECT_NAME}-test tests/test_ecs.cpp)
    target_link_libraries(${PROJECT_NAME}-test PRIVATE ase::ecs)
    target_compile_features(${PROJECT_NAME}-test PRIVATE cxx_std_20)

Weil es keine tests/CMakeLists.txt gab, gab es auch keine Stelle, an der doctest hereinkommt -
und tests/test_ecs.cpp schrieb genau das in seinen eigenen Kopf und meldete es als offenen
Posten, statt ihn zu schliessen. Gemessen war core/ase-ecs das EINZIGE core-Modul ohne eigenes
tests/CMakeLists.txt; die sieben Geschwister (codegen, console, convert, markdown, neo4j,
serial, vault) tragen alle eines.

Alle drei Zeilen leben im Unterverzeichnis weiter: das Ziel heisst dort ase-ecs-test statt
${PROJECT_NAME}-test (ausgeschrieben, wie bei den Geschwistern), ase::ecs bleibt verlinkt, und
cxx_std_20 steht unveraendert. Dazu kommen doctest, ase::math und ein add_test - Erweiterung,
kein Ersatz.
]]
if(ASE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
