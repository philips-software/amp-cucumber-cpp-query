# ---------------------------------------------------------------------------
# CPM – download on first configure if not already cached (standalone only)
# ---------------------------------------------------------------------------
if(CUCUMBER_QUERY_STANDALONE)
    set(CPM_DOWNLOAD_VERSION 0.40.2)
    set(CPM_DOWNLOAD_LOCATION
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

    if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
        message(STATUS "Downloading CPM.cmake ${CPM_DOWNLOAD_VERSION}…")
        file(DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
            "${CPM_DOWNLOAD_LOCATION}"
            TLS_VERIFY ON
        )
    endif()

    include("${CPM_DOWNLOAD_LOCATION}")
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    GIT_TAG v3.11.3
    OPTIONS "JSON_BuildTests OFF" "JSON_Install ON"
    SYSTEM
)
if(nlohmann_json_ADDED)
    # Make the generated config file discoverable by cucumber-messages' find_package()
    list(APPEND CMAKE_PREFIX_PATH "${nlohmann_json_BINARY_DIR}")
endif()

CPMAddPackage(
    NAME cucumber-messages
    GIT_REPOSITORY https://github.com/cucumber/messages.git
    GIT_TAG        main
    SOURCE_SUBDIR  cpp
    SYSTEM
)

CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    OPTIONS
        "INSTALL_GTEST OFF"
        "gtest_force_shared_crt ON"
    SYSTEM
)
