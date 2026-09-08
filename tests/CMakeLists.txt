cmake_minimum_required(VERSION 3.20)

#[[
WHY THIS FILE EXISTS AT ALL - measured 2026-09-01:

core/ase-ecs was the only core module whose test target was declared INLINE in the module
CMakeLists (core/ase-ecs/CMakeLists.txt:193-196). All seven siblings - codegen, console,
convert, markdown, neo4j, serial, vault - carry their own tests/CMakeLists.txt. The inline form
is what kept the target without a doctest dependency: there was no place that pulled one in,
and tests/test_ecs.cpp said so in its own header and reported it as an open item instead of
closing it. This file closes it.

The doctest lookup is the self-contained form of the core siblings (codegen, console, convert,
markdown all do exactly this): find_package first, FetchContent as the fallback. It is a no-op
when a parent scope has already made doctest available, so a tier build pays for it once.
]]
find_package(doctest QUIET)
if(NOT doctest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12
    )
    FetchContent_MakeAvailable(doctest)
endif()

add_executable(ase-ecs-test
    test_ecs.cpp
)

#[[
The doctest entry point is switched on here rather than through a define in the test source, so
the test file stays free of macro definitions. Wording and form are taken from the two
foundation reference targets that already carry it: foundation/ase-math/tests/CMakeLists.txt
and foundation/ase-types/tests/CMakeLists.txt.

THE ONE THING TO CHECK BEFORE COPYING THIS BLOCK ELSEWHERE: target_compile_definitions hands
the macro to EVERY translation unit of the target. With more than one source that means more
than one main(), and it breaks at LINK time - after a green content gate. This target has
exactly one source, so the target-wide form is the equivalent one here. A multi-source target
needs set_source_files_properties on the single source instead.
]]
target_compile_definitions(ase-ecs-test
    PRIVATE
        DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

target_compile_features(ase-ecs-test PRIVATE cxx_std_20)

target_link_libraries(ase-ecs-test
    PRIVATE
        ase::ecs
        #[[
        Layer 0, for ase::math::abs in the two float comparisons of the system tick case.
        ase-ecs itself does not link ase::math - the test does, and only the test.
        ]]
        ase::math
        doctest::doctest
)

add_test(NAME ase-ecs-test COMMAND ase-ecs-test)

set_target_properties(ase-ecs-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
