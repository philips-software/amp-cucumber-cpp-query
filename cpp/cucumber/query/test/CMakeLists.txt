

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
