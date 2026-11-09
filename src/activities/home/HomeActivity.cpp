#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <Memory.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "activities/reader/EpubReaderChapterSelectionActivity.h"
#include "activities/reader/XtcReaderChapterSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookProgress.h"

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Library, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  if (coverGridUi && coverGridUi->isTitlesOnly()) {
    if (RECENT_BOOKS.pruneMissing() && !RECENT_BOOKS.saveToFile()) {
      LOG_ERR("HOME", "Failed to save pruned recent and pinned books");
    }

    const auto& pinnedBooks = RECENT_BOOKS.getPinnedBooks();
    const int displayCount =
        std::clamp(std::max(maxBooks, static_cast<int>(pinnedBooks.size())), 1, CoverGridHomeUi::MAX_TITLE_BOOKS);
    const int recentCount = std::max(0, displayCount - static_cast<int>(pinnedBooks.size()));
    recentBooks.reserve(displayCount);
    const auto loadProgress = [](RecentBook& book) {
      if (book.progressLoaded) return;
      book.progressPercent = loadBookProgress(book.path);
      book.progressLoaded = true;
      RECENT_BOOKS.cacheProgress(book.path, book.progressPercent);
    };

    std::string latestPath;
    for (const RecentBook& book : books) {
      if (!RecentBooksStore::isMissing(book)) {
        latestPath = book.path;
        break;
      }
    }

    for (const RecentBook& book : pinnedBooks) {
      if (static_cast<int>(recentBooks.size()) >= displayCount) break;
      recentBooks.push_back(book);
      recentBooks.back().pinned = true;
      recentBooks.back().latest = book.path == latestPath;
      loadProgress(recentBooks.back());
    }
    int addedRecentBooks = 0;
    for (const RecentBook& book : books) {
      if (addedRecentBooks >= recentCount) break;
      if (RECENT_BOOKS.isPinned(book.path) || RecentBooksStore::isMissing(book)) continue;
      recentBooks.push_back(book);
      recentBooks.back().latest = book.path == latestPath;
      loadProgress(recentBooks.back());
      ++addedRecentBooks;
    }
    return;
  }

  recentBooks.reserve(coverGridUi ? maxBooks : std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRandomHomeBook() {
  if (!coverGridUi || !coverGridUi->isTitlesOnly() || SETTINGS.homeRandomBook == 0) return;

  const size_t visibleSlots = static_cast<size_t>(coverGridUi->maxBooks());
  const size_t visibleCount = std::min(recentBooks.size(), visibleSlots);
  const size_t replacementIndex = visibleCount == visibleSlots ? visibleCount - 1 : recentBooks.size();
  const bool replacingBook = replacementIndex < recentBooks.size();
  if (replacingBook && recentBooks[replacementIndex].pinned) return;

  if (!Storage.exists(library::libraryIndexPath())) {
    LOG_DBG("HOME", "Random book unavailable: library index not built");
    return;
  }

  struct LibraryReader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
  };
  auto reader = makeUniqueNoThrow<LibraryReader>();
  if (!reader) {
    LOG_ERR("HOME", "OOM: random library index");
    return;
  }
  auto& index = reader->index;
  auto& record = reader->record;
  if (!index.open(library::libraryIndexPath())) {
    LOG_ERR("HOME", "Cannot open library index for random Home book");
    return;
  }

  const uint16_t bookCount = index.bookCount();
  if (bookCount == 0) return;

  const uint16_t startRow = static_cast<uint16_t>(random(bookCount));
  const size_t maxAttempts = std::min<size_t>(bookCount, CoverGridHomeUi::MAX_TITLE_BOOKS + 1);
  for (size_t attempt = 0; attempt < maxAttempts; ++attempt) {
    const uint16_t row = static_cast<uint16_t>((startRow + attempt) % bookCount);
    const uint16_t ordinal = index.ordinalForRow(library::SortOrder::RecentDesc, row);
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, record)) {
      if (index.ioFailed()) break;
      continue;
    }

    RecentBook candidate;
    if (!index.readPath(record, candidate.path)) {
      if (index.ioFailed()) break;
      continue;
    }
    if (RecentBooksStore::isMissing(candidate) || RECENT_BOOKS.isPinned(candidate.path)) continue;
    bool alreadyDisplayed = false;
    for (size_t i = 0; i < visibleCount; ++i) {
      if (i != replacementIndex && recentBooks[i].path == candidate.path) {
        alreadyDisplayed = true;
        break;
      }
    }
    if (alreadyDisplayed) continue;

    if (!index.readTitle(record, candidate.title) && !index.readName(record, candidate.title)) continue;
    if (index.ioFailed()) break;
    index.readAuthor(record, candidate.author);
    if (index.ioFailed()) break;

    candidate.random = true;
    candidate.progressPercent = loadBookProgress(candidate.path);
    candidate.progressLoaded = true;
    if (replacingBook) {
      recentBooks[replacementIndex] = std::move(candidate);
    } else {
      recentBooks.push_back(std::move(candidate));
    }
    return;
  }

  if (index.ioFailed()) {
    LOG_ERR("HOME", "Failed reading library index for random Home book");
  } else {
    LOG_DBG("HOME", "No distinct random Home book found");
  }
}

void HomeActivity::fillCoverGridFromLibrary() {
  if (recentBooks.size() >= CoverGridHomeUi::MAX_BOOKS) return;
  // Keep the index and record together off the task stack; reuse for every row.
  struct LibraryReader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
  };
  auto reader = makeUniqueNoThrow<LibraryReader>();
  if (!reader) {
    LOG_ERR("HOME", "OOM: library index");
    return;
  }
  auto& index = reader->index;
  auto& record = reader->record;
  if (!index.open(library::libraryIndexPath())) {
    index.close();
    GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
    library::BuildStats stats;
    if (!library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0) ||
        !index.open(library::libraryIndexPath())) {
      LOG_ERR("HOME", "Cannot populate cover grid from library");
      return;
    }
  }
  for (uint16_t row = 0; row < index.bookCount() && recentBooks.size() < CoverGridHomeUi::MAX_BOOKS; ++row) {
    RecentBook book;
    if (!index.readRecord(index.ordinalForRow(library::SortOrder::RecentDesc, row), record) ||
        !index.readPath(record, book.path))
      continue;
    if (std::any_of(recentBooks.begin(), recentBooks.end(),
                    [&](const RecentBook& existing) { return existing.path == book.path; }) ||
        RecentBooksStore::isMissing(book))
      continue;
    if (!index.readTitle(record, book.title) && !index.readName(record, book.title)) continue;
    index.readAuthor(record, book.author);
    if (index.ioFailed()) break;
    recentBooks.push_back(std::move(book));
  }
}

void HomeActivity::resolveGridCoverPaths() {
  for (auto& book : recentBooks) {
    if (!book.coverBmpPath.empty()) continue;
    // Constructors only derive cache paths; no metadata parsing or image generation.
    // Keep these large objects off the task stack and release each before the next book.
    if (FsHelpers::hasEpubExtension(book.path)) {
      auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
      if (!epub) {
        LOG_ERR("HOME", "OOM: EPUB thumbnail path");
        continue;
      }
      book.coverBmpPath = epub->getThumbBmpPath();
    } else if (FsHelpers::hasXtcExtension(book.path)) {
      auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
      if (!xtc) {
        LOG_ERR("HOME", "OOM: XTC thumbnail path");
        continue;
      }
      book.coverBmpPath = xtc->getThumbBmpPath();
    }
  }
}

void HomeActivity::loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect) {
  if (!book.coverBmpPath.empty() && Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, height).c_str()))
    return;
  // Only one parser lives at a time; EPUB/XTC objects exceed the stack budget.
  if (FsHelpers::hasEpubExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: cover EPUB");
      return;
    }
    book.coverBmpPath = epub->getThumbBmpPath();
    if (Storage.exists(epub->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (epub->generateThumbBmpFromSource(height)) {
      return;
    }
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: cover XTC");
      return;
    }
    book.coverBmpPath = xtc->getThumbBmpPath();
    if (Storage.exists(xtc->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (xtc->load() && xtc->generateThumbBmp(height)) {
      return;
    }
  }
  book.coverBmpPath.clear();
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    // The cover grid shares one slot size; generating at any other height
    // would rescale the dithered thumb at draw time and alias badly.
    const int thumbHeight = coverGridUi ? coverGridUi->thumbHeightFor() : coverHeight;
    if (coverGridUi) {
      loadGridCover(book, thumbHeight, showingLoading, popupRect);
      ++progress;
      if (showingLoading) GUI.fillPopupProgress(renderer, popupRect, progress * 100 / recentBooks.size());
      continue;
    }
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(thumbHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(thumbHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  if (UITheme::getInstance().hasCoverGridHome()) {
    // Screen-lifetime interaction tables and component properties exceed the stack budget.
    coverGridUi = makeUniqueNoThrow<CoverGridHomeUi>(renderer, UITheme::hasTitleListHome());
    if (!coverGridUi) LOG_ERR("HOME", "OOM: cover grid UI; using standard home");
  }
  loadRecentBooks(coverGridUi ? coverGridUi->maxBooks() : metrics.homeRecentBooksCount);
  hasContinueReading = !recentBooks.empty();
  if (coverGridUi) {
    loadRandomHomeBook();
    if (!coverGridUi->isTitlesOnly()) {
      fillCoverGridFromLibrary();
      resolveGridCoverPaths();
    }
    coverGridUi->begin(recentBooks, hasOpdsServers, hasContinueReading);
  }

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  coverGridUi.reset();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  if (bookOptionsPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();

  auto activateSelection = [this] {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::LIBRARY:
        onLibraryOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      default:
        break;
    }
  };

  // Cover grid home splits navigation by button group (see below); the flat
  // next/previous cycle is for the classic list home only.
  if (!coverGridUi) {
    buttonNavigator.onNext([this, menuCount] {
      selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
      requestUpdate();
    });

    buttonNavigator.onPrevious([this, menuCount] {
      selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
      requestUpdate();
    });
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card).
  const auto latestBook = std::find_if(recentBooks.begin(), recentBooks.end(),
                                       [](const RecentBook& book) { return book.latest; });
  const int latestIndex = latestBook != recentBooks.end()
                              ? static_cast<int>(std::distance(recentBooks.begin(), latestBook))
                              : 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && hasContinueReading && !recentBooks.empty()) {
    onSelectBook(recentBooks[latestIndex].path);
    return;
  }

  if (coverGridUi) {
    const int touched = coverGridUi->selectedAction(mappedInput);
    if (touched >= 0 && touched < menuCount) {
      selectorIndex = touched;
      if (coverGridUi->isTitlesOnly() && coverGridUi->wasLongPress()) {
        promptPinBook(touched);
        requestUpdate();
        return;
      }
      activateSelection();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      activateSelection();
      return;
    }
    // Side page buttons walk the covers, front Left/Right walk the tabs
    // (selectorIndex is flat: books first, then the tab items). A press while
    // selection sits in the other band jumps into this band first.
    const int bookCount = static_cast<int>(recentBooks.size());
    const auto cycleBand = [this](const int base, const int count, const int dir) {
      if (count <= 0) return;
      int idx = selectorIndex - base;
      if (idx < 0 || idx >= count) {
        idx = dir > 0 ? 0 : count - 1;
      } else {
        idx = (idx + count + dir) % count;
      }
      selectorIndex = base + idx;
      requestUpdate();
    };
    // Title List draws the latest book at the bottom, so Up moves to older books.
    const int upStep = coverGridUi->isTitlesOnly() ? +1 : -1;
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up},
                                         [&cycleBand, bookCount, upStep] { cycleBand(0, bookCount, upStep); });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down},
                                         [&cycleBand, bookCount, upStep] { cycleBand(0, bookCount, -upStep); });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [&cycleBand, bookCount, menuCount] {
      cycleBand(bookCount, menuCount - bookCount, -1);
    });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [&cycleBand, bookCount, menuCount] {
      cycleBand(bookCount, menuCount - bookCount, +1);
    });
    return;
  }

  const int coverColumnCount = std::max(1, metrics.homeRecentBooksCount);
  const int recentCount = std::min(static_cast<int>(recentBooks.size()), coverColumnCount);
  const int coverColumnWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / coverColumnCount;
  int touchedBook = -1;
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               metrics.homeTopPadding,
                                               metrics.homeTopPadding + metrics.homeCoverTileHeight, coverColumnWidth);
  if (coverTouch != MappedInputManager::RowTouch::None) {
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedBook) {
        selectorIndex = touchedBook;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedBook;
      activateSelection();
    }
    return;
  }

  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  int menuRow = -1;
  // Row height from the theme, not the metrics table: RoundedRaff draws
  // font-derived rows and the touch grid must match the visuals exactly.
  const int menuRowHeight = GUI.getMenuRowHeight(renderer);
  const auto menuTouch = mappedInput.rowTouch(menuRow, menuTop, menuRowHeight + metrics.menuSpacing, renderedMenuCount,
                                              0, INT32_MAX, menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  if (bookOptionsPopup.processRender(renderer, mappedInput)) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  if (coverGridUi) {
    coverGridUi->setSelection(selectorIndex);
    UITheme::getInstance().drawCoverGridHome(*coverGridUi);
    // Front Left/Right walk the tabs, so their hints read Left/Right; the
    // side page buttons (unhinted) walk the covers.
    const auto labels = mappedInput.mapLabels(hasContinueReading ? tr(STR_RESUME) : "", tr(STR_SELECT),
                                              tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH
                                                                   : HalDisplay::FAST_REFRESH);
    // Slot heights are recorded during the draw above; a change (first layout
    // pass, orientation switch) means the paths must point at those sizes and
    // any missing thumbs must be generated. Refreshing the paths right away
    // lets the next pass draw already-cached thumbs before generation runs.
    const bool coverSpecChanged = coverGridUi->takeThumbHeightChanged();
    if (coverSpecChanged) {
      coverGridUi->refreshCoverPaths();
      recentsLoaded = false;
    }
    if (!firstRenderDone) {
      firstRenderDone = true;
      // The second pass only exists to draw thumbnails at their measured size.
      if (!coverGridUi->isTitlesOnly()) requestUpdate();
    } else if (!recentsLoaded && !recentsLoading && !coverGridUi->isTitlesOnly()) {
      loadRecentCovers(CoverGridHomeUi::THUMB_HEIGHT);
      coverGridUi->refreshCoverPaths();
      requestUpdate();
    }
    return;
  }
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  // Band spans topPadding..homeTopPadding: the cover tile starts at the fixed
  // homeTopPadding, so the height must shrink by topPadding or the band (and a
  // centered title, e.g. RoundedRaff's book title) sinks into the tile.
  // Home is the stack root: no back button in its header.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding - metrics.topPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr,
                 nullptr, false);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_LIBRARY), tr(STR_FILE_TRANSFER),
                                        tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Library, Transfer, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Blocks);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  GUI.drawButtonMenu(
      renderer,
      Rect{0, metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset, pageWidth,
           pageHeight - (metrics.headerHeight + metrics.homeTopPadding + metrics.verticalSpacing +
                         metrics.homeMenuTopOffset + metrics.buttonHintsHeight)},
      static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    const int themeThumbHeight = GUI.homeCoverThumbHeight(renderer);
    loadRecentCovers(themeThumbHeight > 0 ? themeThumbHeight : metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { Activity::onSelectBook(path); }

void HomeActivity::openChapterSelector(const RecentBook& book) {
  const auto showUnavailable = [this] {
    const std::vector<std::string> options{tr(STR_OK_BUTTON)};
    bookOptionsPopup.show(StrId::STR_CHAPTERS_UNAVAILABLE, options, 0, {});
    requestUpdate();
  };

  if (FsHelpers::hasEpubExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: chapter selection EPUB");
      showUnavailable();
      return;
    }
    if (!epub->load(false, true) || epub->getTocItemsCount() <= 0) {
      LOG_DBG("HOME", "Chapter metadata unavailable: %s", book.path.c_str());
      showUnavailable();
      return;
    }

    std::shared_ptr<Epub> sharedEpub;
    sharedEpub = std::move(epub);
    auto selector = makeUniqueNoThrow<EpubReaderChapterSelectionActivity>(renderer, mappedInput, sharedEpub, 0);
    if (!selector) {
      LOG_ERR("HOME", "OOM: EPUB chapter selector");
      showUnavailable();
      return;
    }
    startActivityForResult(std::move(selector), [path = book.path](const ActivityResult& result) {
      if (result.isCancelled) return;
      activityManager.goToReaderAtChapter(path, std::get<ChapterResult>(result.data));
    });
    return;
  }

  if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: chapter selection XTC");
      showUnavailable();
      return;
    }
    if (!xtc->load() || !xtc->hasChapters() || xtc->getChapters().empty()) {
      LOG_DBG("HOME", "Chapter metadata unavailable: %s", book.path.c_str());
      showUnavailable();
      return;
    }

    std::shared_ptr<Xtc> sharedXtc;
    sharedXtc = std::move(xtc);
    auto selector = makeUniqueNoThrow<XtcReaderChapterSelectionActivity>(renderer, mappedInput, sharedXtc, 0);
    if (!selector) {
      LOG_ERR("HOME", "OOM: XTC chapter selector");
      showUnavailable();
      return;
    }
    startActivityForResult(std::move(selector), [path = book.path](const ActivityResult& result) {
      if (result.isCancelled) return;
      activityManager.goToReaderAtPage(path, std::get<PageResult>(result.data).page);
    });
    return;
  }

  showUnavailable();
}

void HomeActivity::promptPinBook(const int index) {
  if (!coverGridUi || !coverGridUi->isTitlesOnly() || index < 0 ||
      static_cast<size_t>(index) >= recentBooks.size()) {
    return;
  }
  const RecentBook book = recentBooks[index];
  const size_t slash = book.path.find_last_of('/');
  const char* title = book.title.empty() ? book.path.c_str() + (slash == std::string::npos ? 0 : slash + 1)
                                         : book.title.c_str();
  const bool supportsChapterSelection =
      FsHelpers::hasEpubExtension(book.path) || FsHelpers::hasXtcExtension(book.path);
  const bool canHide = !book.random;
  const int hideOption = canHide ? (supportsChapterSelection ? 2 : 1) : -1;
  const char* options[3] = {I18N.get(book.pinned ? StrId::STR_UNPIN_BOOK : StrId::STR_PIN_BOOK)};
  int optionCount = 1;
  if (supportsChapterSelection) options[optionCount++] = tr(STR_OPEN_CHAPTER);
  if (canHide) options[optionCount++] = tr(STR_HIDE_FROM_HOME);
  bookOptionsPopup.show(tr(STR_BOOK_OPTIONS), title, options, optionCount, 0,
                        [this, book, index, supportsChapterSelection, hideOption](const int option) {
    if (supportsChapterSelection && option == 1) {
      openChapterSelector(book);
      return;
    }

    if (option == hideOption) {
      if (!RECENT_BOOKS.removeFromHome(book)) {
        const std::vector<std::string> errorOptions{tr(STR_OK_BUTTON)};
        bookOptionsPopup.show(StrId::STR_HOME_REMOVE_FAILED, errorOptions, 0, {});
        requestUpdate();
        return;
      }

      RenderLock lock(*this);
      coverGridUi->closeRouting();
      loadRecentBooks(coverGridUi->maxBooks());
      hasContinueReading = !recentBooks.empty();
      loadRandomHomeBook();
      coverGridUi->begin(recentBooks, hasOpdsServers, hasContinueReading);
      selectorIndex = std::min(index, std::max(0, static_cast<int>(recentBooks.size()) - 1));
      requestUpdate();
      return;
    }

    const bool pinning = !book.pinned;
    if (pinning && RECENT_BOOKS.getPinnedBooks().size() >= RecentBooksStore::MAX_PINNED_BOOKS) {
      const std::vector<std::string> errorOptions{tr(STR_OK_BUTTON)};
      bookOptionsPopup.show(StrId::STR_PIN_LIMIT_REACHED, errorOptions, 0, {});
      requestUpdate();
      return;
    }
    if (!RECENT_BOOKS.setPinned(book, pinning)) {
      const std::vector<std::string> errorOptions{tr(STR_OK_BUTTON)};
      bookOptionsPopup.show(StrId::STR_PIN_SAVE_FAILED, errorOptions, 0, {});
      requestUpdate();
      return;
    }

    RenderLock lock(*this);
    coverGridUi->closeRouting();
    loadRecentBooks(coverGridUi->maxBooks());
    hasContinueReading = !recentBooks.empty();
    loadRandomHomeBook();
    coverGridUi->begin(recentBooks, hasOpdsServers, hasContinueReading);
    const auto selected = std::find_if(recentBooks.begin(), recentBooks.end(),
                                       [&](const RecentBook& candidate) { return candidate.path == book.path; });
    selectorIndex = selected != recentBooks.end()
                        ? static_cast<int>(std::distance(recentBooks.begin(), selected))
                        : 0;
    requestUpdate();
  });
}

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onLibraryOpen() { activityManager.goToLibrary(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
