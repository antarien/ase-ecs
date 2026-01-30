cmake_minimum_required(VERSION 3.20)
project(ase-ecs VERSION 0.1.0 LANGUAGES CXX)

#[[
ASE CORE BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-ecs
@description ECS framework - EnTT wrapper with App, System, Registry.
             Core Entity-Component-System architecture.

@module      ase-ecs
@layer       1 (Core)
@created     2025-01-01
@modified    2025-01-30
@version     1.0.0

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

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

add_library(${PROJECT_NAME} STATIC
    #[[
    CORE
    ECS framework core: System base class, App runner.
    ]]
    src/system.cpp
    src/app.cpp
)
add_library(ase::ecs ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        EnTT::EnTT       # External - Entity Component System
        ase::log         # Layer 1 - Logging
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
