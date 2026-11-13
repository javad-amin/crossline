add_executable(ReaderImageRefreshPolicyTest
  ReaderImageRefreshPolicyTest.cpp
)

target_include_directories(ReaderImageRefreshPolicyTest PRIVATE
  ${REPO_ROOT}/src/activities/reader
)

target_link_libraries(ReaderImageRefreshPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReaderImageRefreshPolicyTest)
