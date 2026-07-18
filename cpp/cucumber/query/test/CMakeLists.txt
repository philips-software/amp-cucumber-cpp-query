

add_executable(cucumber_query_lib.test
    QueryTest.cpp
)

target_link_libraries(cucumber_query_lib.test
    PRIVATE
        cucumber::query
        GTest::gtest_main
        GTest::gmock
)

target_compile_definitions(cucumber_query_lib.test
    PRIVATE
        TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../../testdata/src"
)

include(GoogleTest)
gtest_discover_tests(cucumber_query_lib.test)

if(CUCUMBER_QUERY_ENABLE_MUTATION)
    add_custom_target(mutation-test
        COMMAND ${MULL_RUNNER} $<TARGET_FILE:cucumber_query_lib.test> --mutation-score-threshold 85
        DEPENDS cucumber_query_lib.test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
    )
endif()
