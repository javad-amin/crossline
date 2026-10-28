#include <gtest/gtest.h>

#include "ReaderTools.h"

TEST(ReaderToolsTest, ToolbarOrderPutsBookmarksBeforeMore) {
  EXPECT_EQ(0, READER_TOOL_CONTENTS);
  EXPECT_EQ(1, READER_TOOL_TEXT);
  EXPECT_EQ(2, READER_TOOL_BOOKMARK);
  EXPECT_EQ(3, READER_TOOL_LATEST_BOOKMARK);
  EXPECT_EQ(4, READER_TOOL_MORE);
  EXPECT_EQ(5, READER_TOOL_COUNT);
}

TEST(ReaderToolsTest, OnlyContentsTextAndMoreOpenPanels) {
  EXPECT_TRUE(readerToolOpensPanel(READER_TOOL_CONTENTS));
  EXPECT_TRUE(readerToolOpensPanel(READER_TOOL_TEXT));
  EXPECT_TRUE(readerToolOpensPanel(READER_TOOL_MORE));
  EXPECT_FALSE(readerToolOpensPanel(READER_TOOL_BOOKMARK));
  EXPECT_FALSE(readerToolOpensPanel(READER_TOOL_LATEST_BOOKMARK));
  EXPECT_FALSE(readerToolOpensPanel(-1));
  EXPECT_FALSE(readerToolOpensPanel(READER_TOOL_COUNT));
}

TEST(ReaderToolsTest, FocusWrapsAroundAllTools) {
  EXPECT_EQ(1, stepReaderTool(0, +1));
  EXPECT_EQ(0, stepReaderTool(READER_TOOL_MORE, +1));
  EXPECT_EQ(READER_TOOL_MORE, stepReaderTool(0, -1));
  EXPECT_EQ(READER_TOOL_LATEST_BOOKMARK, stepReaderTool(READER_TOOL_MORE, -1));
}
