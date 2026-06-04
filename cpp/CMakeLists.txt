cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(cucumber-query LANGUAGES CXX VERSION 0.1.0)

# ---------------------------------------------------------------------------
# Standalone detection
# ---------------------------------------------------------------------------
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CUCUMBER_QUERY_STANDALONE ON)
else()
    set(CUCUMBER_QUERY_STANDALONE OFF)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CUCUMBER_QUERY_ENABLE_COVERAGE "Enable test coverage instrumentation" OFF)
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
include("dependencies.cpm")

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(cucumber.query
    cucumber/query/Query.cpp
    cucumber/query/Lineage.cpp
)

add_library(cucumber::query ALIAS cucumber.query)

target_include_directories(cucumber.query
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cucumber.query
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
    target_compile_options(cucumber.query PUBLIC --coverage)
    target_link_options(cucumber.query PUBLIC --coverage)
endif()

# ---------------------------------------------------------------------------
# Optional: tests
# ---------------------------------------------------------------------------
if(CUCUMBER_QUERY_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(cucumber/query/test)
endif()
