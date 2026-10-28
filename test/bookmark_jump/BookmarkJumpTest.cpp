#include <gtest/gtest.h>

#include "BookmarkJump.h"

namespace {
BookmarkEntry makeBookmark(const char* xpath, float percentage, uint16_t spine, uint16_t page, uint16_t pages) {
  BookmarkEntry b;
  b.xpath = xpath;
  b.percentage = percentage;
  b.computedSpineIndex = spine;
  b.computedChapterProgress = page;
  b.computedChapterPageCount = pages;
  return b;
}
}  // namespace

TEST(BookmarkJumpTest, CarriesPositionAndPageHints) {
  BookmarkEntry b = makeBookmark("/body/p[3]", 0.42f, 5, 7, 20);
  b.hasVisibleTextOffset = true;
  b.visibleTextOffset = 1234;
  const ProgressChangeResult r = progressChangeForBookmark(b, 30);
  EXPECT_EQ("/body/p[3]", r.xpath);
  EXPECT_FLOAT_EQ(0.42f, r.percentage);
  EXPECT_TRUE(r.hasSavedProgress);
  EXPECT_TRUE(r.hasVisibleTextOffset);
  EXPECT_EQ(1234u, r.visibleTextOffset);
  EXPECT_EQ(5, r.spineIndex);
  EXPECT_EQ(7, r.page);
  EXPECT_EQ(20, r.totalPages);
}

TEST(BookmarkJumpTest, DropsStalePageHintsButKeepsSpine) {
  const ProgressChangeResult pastEnd = progressChangeForBookmark(makeBookmark("x", 0.1f, 40, 1, 5), 30);
  EXPECT_EQ(40, pastEnd.spineIndex);
  EXPECT_EQ(0, pastEnd.page);
  EXPECT_EQ(0, pastEnd.totalPages);

  const ProgressChangeResult badPage = progressChangeForBookmark(makeBookmark("x", 0.1f, 2, 9, 5), 30);
  EXPECT_EQ(0, badPage.page);
  EXPECT_EQ(0, badPage.totalPages);
}

TEST(BookmarkJumpTest, LatestIsTheMostRecentlyAdded) {
  std::vector<BookmarkEntry> bookmarks;
  EXPECT_EQ(nullptr, latestBookmark(bookmarks));
  bookmarks.push_back(makeBookmark("newest", 0.5f, 1, 0, 1));
  bookmarks.push_back(makeBookmark("older", 0.2f, 1, 0, 1));
  ASSERT_NE(nullptr, latestBookmark(bookmarks));
  EXPECT_EQ("newest", latestBookmark(bookmarks)->xpath);
}
