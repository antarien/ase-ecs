cmake_minimum_required(VERSION 3.20)

project(ase-ecs
    VERSION 0.1.0
    DESCRIPTION "ASE Entity Component System - EnTT wrapper"
    LANGUAGES CXX
)

# Static library
add_library(ase-ecs STATIC
    src/system_registry.cpp
    src/schedule_registry.cpp
    src/ecs.cpp
    src/app.cpp
)
add_library(ase::ecs ALIAS ase-ecs)

target_include_directories(ase-ecs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ase-ecs PUBLIC cxx_std_20)

# Link EnTT
target_link_libraries(ase-ecs PUBLIC EnTT::EnTT)

# Tests
if(ASE_BUILD_TESTS)
    add_executable(ase-ecs-test tests/test_ecs.cpp)
    target_link_libraries(ase-ecs-test PRIVATE ase::ecs)
    target_compile_features(ase-ecs-test PRIVATE cxx_std_20)
endif()
