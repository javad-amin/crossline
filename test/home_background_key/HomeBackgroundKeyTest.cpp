#include <gtest/gtest.h>

#include "HomeBackgroundKey.h"

TEST(HomeBackgroundKeyTest, IdentifiesImageByPathSizeTimeAndInversion) {
  char key[HOME_BACKGROUND_KEY_LEN];
  ASSERT_TRUE(makeHomeBackgroundKey(key, sizeof(key), "/sleep/wolf.bmp", 1152054, 1761600000, false));
  EXPECT_STREQ("/sleep/wolf.bmp|1152054|1761600000|0", key);
  ASSERT_TRUE(makeHomeBackgroundKey(key, sizeof(key), "/sleep.bmp", 10, 0, true));
  EXPECT_STREQ("/sleep.bmp|10|0|1", key);
}

TEST(HomeBackgroundKeyTest, SameSizedReplacementImageGetsANewKey) {
  char before[HOME_BACKGROUND_KEY_LEN];
  char after[HOME_BACKGROUND_KEY_LEN];
  makeHomeBackgroundKey(before, sizeof(before), "/sleep.bmp", 1152054, 1761600000, false);
  makeHomeBackgroundKey(after, sizeof(after), "/sleep.bmp", 1152054, 1761700000, false);
  EXPECT_FALSE(homeBackgroundReusable(before, after));
}

TEST(HomeBackgroundKeyTest, RejectsMissingPathOrTooLongKey) {
  char key[HOME_BACKGROUND_KEY_LEN];
  EXPECT_FALSE(makeHomeBackgroundKey(key, sizeof(key), nullptr, 1, 0, false));
  EXPECT_FALSE(makeHomeBackgroundKey(key, sizeof(key), "", 1, 0, false));
  EXPECT_STREQ("", key);
  char tiny[8];
  EXPECT_FALSE(makeHomeBackgroundKey(tiny, sizeof(tiny), "/sleep/a-long-name.bmp", 1, 0, false));
  EXPECT_STREQ("", tiny);
}

TEST(HomeBackgroundKeyTest, ReusesOnlyWhenKeysMatchAndAreKnown) {
  EXPECT_TRUE(homeBackgroundReusable("/sleep.bmp|10|0", "/sleep.bmp|10|0"));
  EXPECT_FALSE(homeBackgroundReusable("/sleep.bmp|10|0", "/sleep.bmp|11|0"));
  EXPECT_FALSE(homeBackgroundReusable("/sleep.bmp|10|0", "/sleep.bmp|10|1"));
  EXPECT_FALSE(homeBackgroundReusable("", ""));
  EXPECT_FALSE(homeBackgroundReusable("/sleep.bmp|10|0", ""));
}
