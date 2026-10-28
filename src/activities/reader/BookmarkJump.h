#pragma once

#include <vector>

#include "BookmarkEntry.h"
#include "activities/ActivityResult.h"

// Bookmarks are stored newest first.
inline const BookmarkEntry* latestBookmark(const std::vector<BookmarkEntry>& bookmarks) {
  return bookmarks.empty() ? nullptr : &bookmarks.front();
}

// Jump target for a bookmark. Page hints are kept only while they still fit the book.
inline ProgressChangeResult progressChangeForBookmark(const BookmarkEntry& bookmark, const int spineCount) {
  ProgressChangeResult result{};
  result.xpath = bookmark.xpath;
  result.percentage = bookmark.percentage;
  result.hasSavedProgress = true;
  result.hasVisibleTextOffset = bookmark.hasVisibleTextOffset;
  result.visibleTextOffset = bookmark.visibleTextOffset;
  // The offset is spine-relative, so carry its spine even when the page hints are stale;
  // the reader validates the index.
  result.spineIndex = bookmark.computedSpineIndex;
  if (bookmark.computedChapterPageCount > 0 && bookmark.computedChapterProgress < bookmark.computedChapterPageCount &&
      bookmark.computedSpineIndex < spineCount) {
    result.page = bookmark.computedChapterProgress;
    result.totalPages = bookmark.computedChapterPageCount;
  }
  return result;
}
