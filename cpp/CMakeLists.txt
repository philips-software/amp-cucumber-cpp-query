cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(cucumber-query LANGUAGES CXX VERSION 0.1.0)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
if (POLICY CMP0077)
    # Allow CMake 3.13+ to override options when using FetchContent / add_subdirectory.
    cmake_policy(SET CMP0077 NEW)
endif ()

option(CUCUMBER_QUERY_FETCH_DEPS "Fetch dependencies via FetchContent." Off)
option(CUCUMBER_QUERY_ENABLE_COVERAGE "Enable test coverage instrumentation" OFF)
option(CUCUMBER_QUERY_ENABLE_MUTATION "Enable mutation testing instrumentation with Mull" OFF)
option(CUCUMBER_QUERY_BUILD_TESTS "Build cucumber-query tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler cache
# ---------------------------------------------------------------------------
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
else()
    find_program(SCCACHE_PROGRAM sccache)
    if(SCCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
        message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(cmake/dependencies.cpm.cmake)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(cucumber_query_lib)
add_library(cucumber::query ALIAS cucumber_query_lib)

target_sources(cucumber_query_lib
    PRIVATE
        cucumber/query/Query.cpp
        cucumber/query/Lineage.cpp
)

target_include_directories(cucumber_query_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cucumber_query_lib
    PUBLIC
        cucumber::messages
)

# ---------------------------------------------------------------------------
# Coverage
# ---------------------------------------------------------------------------
if(CUCUMBER_QUERY_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Coverage instrumentation is only supported with GCC or Clang")
    endif()
    target_compile_options(cucumber_query_lib PUBLIC --coverage)
    target_link_options(cucumber_query_lib PUBLIC --coverage)
endif()

# ---------------------------------------------------------------------------
# Mutation testing
# ---------------------------------------------------------------------------
if(CUCUMBER_QUERY_ENABLE_MUTATION)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Mutation testing with Mull requires Clang")
    endif()
    find_program(MULL_RUNNER mull-runner REQUIRED)
    find_file(MULL_IR_FRONTEND mull-ir-frontend PATHS /usr/lib NO_DEFAULT_PATH REQUIRED)
    target_compile_options(cucumber_query_lib PUBLIC
        -O0
        -fpass-plugin=${MULL_IR_FRONTEND}
        -g
        -grecord-command-line
    )
endif()

# ---------------------------------------------------------------------------
# Optional: tests
# ---------------------------------------------------------------------------
if(CUCUMBER_QUERY_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(cucumber/query/test)
endif()
