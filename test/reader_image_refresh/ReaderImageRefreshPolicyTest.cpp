#include <gtest/gtest.h>

#include "ReaderImageRefreshPolicy.h"

TEST(ReaderImageRefreshPolicyTest, UsesCoverageSelectedOverlayForSupportedUc8279Images) {
  EXPECT_TRUE(readerUsesOverlayGrayscaleForImage(true, false, true, true));
}

TEST(ReaderImageRefreshPolicyTest, DoesNotUseImageOverlayOnOtherPagesOrPanels) {
  EXPECT_FALSE(readerUsesOverlayGrayscaleForImage(false, false, true, true));
  EXPECT_FALSE(readerUsesOverlayGrayscaleForImage(true, true, true, true));
  EXPECT_FALSE(readerUsesOverlayGrayscaleForImage(true, false, false, true));
  EXPECT_FALSE(readerUsesOverlayGrayscaleForImage(true, false, true, false));
}

TEST(ReaderImageRefreshPolicyTest, DefersScheduledCleanupForGrayscaleImagePage) {
  const auto policy = readerImageRefreshPolicy(false, 10);

  EXPECT_FALSE(policy.useHalfRefresh);
  EXPECT_EQ(10, policy.pagesUntilFullRefresh);
}

TEST(ReaderImageRefreshPolicyTest, PreservesManualCleanupAndRestartsCadence) {
  const auto policy = readerImageRefreshPolicy(true, 10);

  EXPECT_TRUE(policy.useHalfRefresh);
  EXPECT_EQ(10, policy.pagesUntilFullRefresh);
}
