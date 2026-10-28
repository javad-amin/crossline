#include <gtest/gtest.h>

#include "SleepSources.h"

TEST(SleepSourcesTest, NothingPresentByDefault) {
  SleepSources sources;
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_BMP));
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_DIR));
  EXPECT_FALSE(sources.has(SleepSources::OVERLAY_BMP));
}

TEST(SleepSourcesTest, RecordsFilesAndDirectoriesByKind) {
  SleepSources sources;
  sources.noteRootEntry("sleep.bmp", false);
  sources.noteRootEntry("sleep", true);
  sources.noteRootEntry(".sleep-overlay", true);
  EXPECT_TRUE(sources.has(SleepSources::SLEEP_BMP));
  EXPECT_TRUE(sources.has(SleepSources::SLEEP_DIR));
  EXPECT_TRUE(sources.has(SleepSources::OVERLAY_HIDDEN_DIR));
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_HIDDEN_DIR));
  EXPECT_FALSE(sources.has(SleepSources::OVERLAY_DIR));
}

TEST(SleepSourcesTest, AFileDoesNotCountAsADirectoryOrViceVersa) {
  SleepSources sources;
  sources.noteRootEntry("sleep", false);
  sources.noteRootEntry("sleep.bmp", true);
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_DIR));
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_BMP));
}

TEST(SleepSourcesTest, MatchesNamesCaseInsensitivelyLikeFat) {
  SleepSources sources;
  sources.noteRootEntry("Sleep.BMP", false);
  sources.noteRootEntry("SLEEP-OVERLAY.PNG", false);
  EXPECT_TRUE(sources.has(SleepSources::SLEEP_BMP));
  EXPECT_TRUE(sources.has(SleepSources::OVERLAY_PNG));
}

TEST(SleepSourcesTest, IgnoresUnrelatedEntries) {
  SleepSources sources;
  sources.noteRootEntry("books", true);
  sources.noteRootEntry("sleep.bmp.bak", false);
  sources.noteRootEntry("ssleep.bmp", false);
  EXPECT_FALSE(sources.has(SleepSources::SLEEP_BMP));
  EXPECT_FALSE(sources.any());
}

TEST(SleepSourcesTest, UnknownMeansEverythingMayExist) {
  SleepSources sources = SleepSources::unknown();
  EXPECT_TRUE(sources.has(SleepSources::SLEEP_BMP));
  EXPECT_TRUE(sources.has(SleepSources::OVERLAY_DIR));
}
