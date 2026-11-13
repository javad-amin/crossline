#include "RecentBooksStore.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Xtc.h>

#include <algorithm>
#include <iterator>
#include <utility>

void RecentBooksStore::toJson(JsonDocument& doc) const {
  auto writeBooks = [](JsonArray arr, const std::vector<RecentBook>& books) {
    for (const auto& book : books) {
      JsonObject obj = arr.add<JsonObject>();
      obj["path"] = book.path;
      obj["title"] = book.title;
      obj["author"] = book.author;
      obj["coverBmpPath"] = book.coverBmpPath;
    }
  };
  writeBooks(doc["books"].to<JsonArray>(), recentBooks);
  writeBooks(doc["pinnedBooks"].to<JsonArray>(), pinnedBooks);
}

bool RecentBooksStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'books' key (treat as empty list); only a
  // JSON parse error is fatal. A null JsonArray iterates zero times.
  recentBooks.clear();
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  recentBooks.reserve(std::min(arr.size(), static_cast<size_t>(MAX_RECENT_BOOKS)));
  for (JsonObjectConst obj : arr) {
    if (getCount() >= MAX_RECENT_BOOKS) break;
    RecentBook book;
    book.path = obj["path"] | "";
    book.title = obj["title"] | "";
    book.author = obj["author"] | "";
    book.coverBmpPath = obj["coverBmpPath"] | "";
    recentBooks.push_back(book);
  }

  pinnedBooks.clear();
  JsonArrayConst pinnedArr = doc["pinnedBooks"].as<JsonArrayConst>();
  pinnedBooks.reserve(MAX_PINNED_BOOKS);
  for (JsonObjectConst obj : pinnedArr) {
    if (pinnedBooks.size() >= MAX_PINNED_BOOKS) break;
    RecentBook book;
    book.path = obj["path"] | "";
    if (book.path.empty() || isPinned(book.path)) continue;
    book.title = obj["title"] | "";
    book.author = obj["author"] | "";
    book.coverBmpPath = obj["coverBmpPath"] | "";
    book.pinned = true;
    pinnedBooks.push_back(std::move(book));
  }

  LOG_DBG("RBS", "Recent books loaded from file (%d entries)", getCount());
  return true;
}

bool RecentBooksStore::setPinned(const RecentBook& book, const bool pinned) {
  const auto it = std::find_if(pinnedBooks.begin(), pinnedBooks.end(),
                               [&](const RecentBook& candidate) { return candidate.path == book.path; });
  if (pinned) {
    if (it != pinnedBooks.end()) {
      if (it->title == book.title && it->author == book.author && it->coverBmpPath == book.coverBmpPath) return true;
      const RecentBook previous = *it;
      it->title = book.title;
      it->author = book.author;
      it->coverBmpPath = book.coverBmpPath;
      it->pinned = true;
      if (saveToFile()) return true;
      *it = previous;
      return false;
    }
    if (pinnedBooks.size() >= MAX_PINNED_BOOKS) {
      LOG_ERR("RBS", "Cannot pin more than %d books", MAX_PINNED_BOOKS);
      return false;
    }
    pinnedBooks.reserve(MAX_PINNED_BOOKS);
    RecentBook copy = book;
    copy.pinned = true;
    pinnedBooks.push_back(std::move(copy));
    if (saveToFile()) return true;
    pinnedBooks.pop_back();
    return false;
  }

  if (it == pinnedBooks.end()) return true;
  const size_t index = static_cast<size_t>(std::distance(pinnedBooks.begin(), it));
  RecentBook previous = *it;
  pinnedBooks.erase(it);
  if (saveToFile()) return true;
  pinnedBooks.insert(pinnedBooks.begin() + index, std::move(previous));
  return false;
}

bool RecentBooksStore::isPinned(const std::string& path) const {
  return std::any_of(pinnedBooks.begin(), pinnedBooks.end(),
                     [&](const RecentBook& book) { return book.path == path; });
}

void RecentBooksStore::cacheProgress(const std::string& path, const int progressPercent) {
  const auto cache = [&](std::vector<RecentBook>& books) {
    const auto it = std::find_if(books.begin(), books.end(), [&](const RecentBook& book) { return book.path == path; });
    if (it == books.end()) return;
    it->progressPercent = progressPercent;
    it->progressLoaded = true;
  };
  cache(recentBooks);
  cache(pinnedBooks);
}

bool RecentBooksStore::removeFromHome(const RecentBook& book) {
  const auto recentIt = std::find_if(recentBooks.begin(), recentBooks.end(),
                                     [&](const RecentBook& recent) { return recent.path == book.path; });
  const auto pinnedIt = std::find_if(pinnedBooks.begin(), pinnedBooks.end(),
                                     [&](const RecentBook& pinned) { return pinned.path == book.path; });
  if (recentIt == recentBooks.end() && pinnedIt == pinnedBooks.end()) return true;

  const bool wasRecent = recentIt != recentBooks.end();
  const bool wasPinned = pinnedIt != pinnedBooks.end();
  const size_t recentIndex = wasRecent ? static_cast<size_t>(std::distance(recentBooks.begin(), recentIt)) : 0;
  const size_t pinnedIndex = wasPinned ? static_cast<size_t>(std::distance(pinnedBooks.begin(), pinnedIt)) : 0;
  if (wasRecent) recentBooks.erase(recentIt);
  if (wasPinned) pinnedBooks.erase(pinnedIt);
  if (saveToFile()) return true;

  if (wasRecent) recentBooks.insert(recentBooks.begin() + recentIndex, book);
  if (wasPinned) {
    RecentBook pinnedBook = book;
    pinnedBook.pinned = true;
    pinnedBooks.insert(pinnedBooks.begin() + pinnedIndex, std::move(pinnedBook));
  }
  LOG_ERR("RBS", "Failed to persist home removal: %s", book.path.c_str());
  return false;
}

void RecentBooksStore::removeDeletedBook(const std::string& path) {
  const size_t recentCount = recentBooks.size();
  const size_t pinnedCount = pinnedBooks.size();
  recentBooks.erase(std::remove_if(recentBooks.begin(), recentBooks.end(),
                                   [&](const RecentBook& book) { return book.path == path; }),
                    recentBooks.end());
  pinnedBooks.erase(std::remove_if(pinnedBooks.begin(), pinnedBooks.end(),
                                   [&](const RecentBook& book) { return book.path == path; }),
                    pinnedBooks.end());
  if ((recentBooks.size() != recentCount || pinnedBooks.size() != pinnedCount) && !saveToFile()) {
    LOG_ERR("RBS", "Failed to persist deleted book removal: %s", path.c_str());
  }
}

void RecentBooksStore::addBook(const std::string& path, const std::string& title, const std::string& author,
                               const std::string& coverBmpPath) {
  // Drop stale entries first so a new add can't evict a valid book in their stead.
  pruneMissing();

  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it != recentBooks.end()) {
    // Move the existing entry to the front instead of copying it. The reader calls this after its first
    // render, so fresh string buffers would land among reader allocations and outlive them, splitting the
    // largest free block once the reader exits.
    std::rotate(recentBooks.begin(), it, it + 1);
    RecentBook& book = recentBooks.front();
    if (book.title != title) book.title = title;
    if (book.author != author) book.author = author;
    if (book.coverBmpPath != coverBmpPath) book.coverBmpPath = coverBmpPath;
    book.progressLoaded = false;
  } else {
    recentBooks.insert(recentBooks.begin(), {path, title, author, coverBmpPath});
    if (recentBooks.size() > MAX_RECENT_BOOKS) {
      recentBooks.resize(MAX_RECENT_BOOKS);
    }
  }

  const auto pinned = std::find_if(pinnedBooks.begin(), pinnedBooks.end(),
                                   [&](const RecentBook& book) { return book.path == path; });
  if (pinned != pinnedBooks.end()) {
    pinned->title = title;
    pinned->author = author;
    pinned->coverBmpPath = coverBmpPath;
    pinned->progressLoaded = false;
    pinned->pinned = true;
  }

  saveToFile();
}

void RecentBooksStore::updateBook(const std::string& path, const std::string& title, const std::string& author,
                                  const std::string& coverBmpPath) {
  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  const auto pinned =
      std::find_if(pinnedBooks.begin(), pinnedBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it != recentBooks.end() || pinned != pinnedBooks.end()) {
    if (it != recentBooks.end()) {
      RecentBook& book = *it;
      book.title = title;
      book.author = author;
      book.coverBmpPath = coverBmpPath;
    }
    if (pinned != pinnedBooks.end()) {
      pinned->title = title;
      pinned->author = author;
      pinned->coverBmpPath = coverBmpPath;
    }
    saveToFile();
  }
}

bool RecentBooksStore::removeByPath(const std::string& path) {
  auto it =
      std::find_if(recentBooks.begin(), recentBooks.end(), [&](const RecentBook& book) { return book.path == path; });
  if (it == recentBooks.end()) {
    return false;
  }
  recentBooks.erase(it);
  if (!saveToFile()) {
    LOG_ERR("RBS", "Failed to persist removal of recent book: %s", path.c_str());
  }
  return true;
}

void RecentBooksStore::updatePath(const std::string& oldPath, const std::string& newPath,
                                  const std::string& oldCachePath, const std::string& newCachePath) {
  bool changed = false;
  auto update = [&](std::vector<RecentBook>& books) {
    const auto it =
        std::find_if(books.begin(), books.end(), [&](const RecentBook& book) { return book.path == oldPath; });
    if (it == books.end()) return;
    it->path = newPath;
    if (!oldCachePath.empty() && !it->coverBmpPath.empty() && it->coverBmpPath.rfind(oldCachePath, 0) == 0) {
      it->coverBmpPath = newCachePath + it->coverBmpPath.substr(oldCachePath.size());
    }
    changed = true;
  };
  update(recentBooks);
  update(pinnedBooks);
  if (!changed) return;
  saveToFile();
}

bool RecentBooksStore::isMissing(const RecentBook& book) { return !Storage.exists(book.path.c_str()); }

bool RecentBooksStore::pruneMissing() {
  const size_t recentCount = recentBooks.size();
  const size_t pinnedCount = pinnedBooks.size();
  recentBooks.erase(std::remove_if(recentBooks.begin(), recentBooks.end(), &isMissing), recentBooks.end());
  pinnedBooks.erase(std::remove_if(pinnedBooks.begin(), pinnedBooks.end(), &isMissing), pinnedBooks.end());
  return recentBooks.size() != recentCount || pinnedBooks.size() != pinnedCount;
}

RecentBook RecentBooksStore::getDataFromBook(std::string path) const {
  std::string lastBookFileName = "";
  const size_t lastSlash = path.find_last_of('/');
  if (lastSlash != std::string::npos) {
    lastBookFileName = path.substr(lastSlash + 1);
  }

  LOG_DBG("RBS", "Loading recent book: %s", path.c_str());

  // If epub, try to load the metadata for title/author and cover.
  // Use buildIfMissing=false to avoid heavy epub loading on boot; getTitle()/getAuthor() may be
  // blank until the book is opened, and entries with missing title are omitted from recent list.
  if (FsHelpers::hasEpubExtension(lastBookFileName)) {
    Epub epub(path, "/.crosspoint");
    epub.load(false, true);
    return RecentBook{path, epub.getTitle(), epub.getAuthor(), epub.getThumbBmpPath()};
  } else if (FsHelpers::hasXtcExtension(lastBookFileName)) {
    // Handle XTC file
    Xtc xtc(path, "/.crosspoint");
    if (xtc.load()) {
      return RecentBook{path, xtc.getTitle(), xtc.getAuthor(), xtc.getThumbBmpPath()};
    }
  } else if (FsHelpers::hasTxtExtension(lastBookFileName) || FsHelpers::hasMarkdownExtension(lastBookFileName)) {
    return RecentBook{path, lastBookFileName, "", ""};
  }
  return RecentBook{path, "", "", ""};
}
