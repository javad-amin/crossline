add_executable(HomeBackgroundKeyTest
  HomeBackgroundKeyTest.cpp
)

target_include_directories(HomeBackgroundKeyTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(HomeBackgroundKeyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(HomeBackgroundKeyTest)
