add_executable(ReaderToolsTest
  ReaderToolsTest.cpp
)

target_include_directories(ReaderToolsTest PRIVATE
  ${REPO_ROOT}/src/activities/reader
)

target_link_libraries(ReaderToolsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReaderToolsTest)
