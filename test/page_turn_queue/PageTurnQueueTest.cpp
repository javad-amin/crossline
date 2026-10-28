#include <gtest/gtest.h>

#include "PageTurnQueue.h"

TEST(PageTurnQueueTest, StartsEmpty) {
  PageTurnQueue queue;
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(0, queue.take());
}

TEST(PageTurnQueueTest, AccumulatesPressesInTheSameDirection) {
  PageTurnQueue queue;
  queue.add(true);
  queue.add(true);
  queue.add(true);
  EXPECT_EQ(3, queue.take());
  EXPECT_TRUE(queue.empty());
}

TEST(PageTurnQueueTest, OppositePressesCancelOut) {
  PageTurnQueue queue;
  queue.add(true);
  queue.add(true);
  queue.add(false);
  EXPECT_EQ(1, queue.take());
  queue.add(true);
  queue.add(false);
  EXPECT_TRUE(queue.empty());
}

TEST(PageTurnQueueTest, ClampsToMaximumInEitherDirection) {
  PageTurnQueue queue;
  for (int i = 0; i < 50; i++) queue.add(true);
  EXPECT_EQ(PageTurnQueue::MAX_PENDING, queue.take());
  for (int i = 0; i < 50; i++) queue.add(false);
  EXPECT_EQ(-PageTurnQueue::MAX_PENDING, queue.take());
}

TEST(PageTurnQueueTest, TakeClearsTheQueue) {
  PageTurnQueue queue;
  queue.add(false);
  EXPECT_EQ(-1, queue.take());
  EXPECT_EQ(0, queue.take());
}
