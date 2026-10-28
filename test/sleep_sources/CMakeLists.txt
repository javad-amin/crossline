add_executable(SleepSourcesTest
  SleepSourcesTest.cpp
)

target_include_directories(SleepSourcesTest PRIVATE
  ${REPO_ROOT}/src/activities/boot_sleep
)

target_link_libraries(SleepSourcesTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SleepSourcesTest)
