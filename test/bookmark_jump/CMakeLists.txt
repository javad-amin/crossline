add_executable(BookmarkJumpTest
  BookmarkJumpTest.cpp
)

target_include_directories(BookmarkJumpTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/activities/reader
  ${REPO_ROOT}/src/activities
)

target_link_libraries(BookmarkJumpTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BookmarkJumpTest)
