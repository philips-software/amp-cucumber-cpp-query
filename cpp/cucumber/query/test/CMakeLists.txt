

add_executable(cucumber.query.test
    QueryTest.cpp
)

target_link_libraries(cucumber.query.test
    PRIVATE
        cucumber::query
        GTest::gtest_main
        GTest::gmock
)

target_compile_definitions(cucumber.query.test
    PRIVATE
        TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../../testdata/src"
)

include(GoogleTest)
gtest_discover_tests(cucumber.query.test)

if(CUCUMBER_QUERY_ENABLE_MUTATION)
    add_custom_target(mutation-test
        COMMAND ${MULL_RUNNER} $<TARGET_FILE:cucumber.query.test> --mutation-score-threshold 85
        DEPENDS cucumber.query.test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
    )
endif()
