#include "NonogramsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "NonogramsProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

NonogramsActivity::NonogramsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Nonograms", renderer, mappedInput) {}

void NonogramsActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis());
  if (rngState == 0) rngState = 1;
  auto& progress = NonogramsProgressStore::getInstance();
  if (progress.loadFromFile() && progress.hasSavedGame()) {
    hasSavedGame = true;
    n = progress.n();
    difficulty = static_cast<Difficulty>(progress.difficulty());
    solution = progress.solution();
    for (int i = 0; i < MAX_N * MAX_N; ++i) {
      grid[i] = static_cast<Mark>(progress.grid()[i]);
    }
    cursorX = progress.cursorX();
    cursorY = progress.cursorY();
    win = progress.win();
    screen = Screen::StartMenu;
    startSelection = 0;
  } else {
    screen = Screen::Difficulty;
    difficulty = Difficulty::Normal;
  }
  needsRedraw = true;
  requestUpdate();
}

int NonogramsActivity::gridSize() const {
  switch (difficulty) {
    case Difficulty::Easy:
      return 5;
    case Difficulty::Normal:
      return 10;
    case Difficulty::Hard:
      return 15;
  }
  return 10;
}

void NonogramsActivity::generatePuzzle() {
  auto rnd = [&]() -> uint32_t {
    rngState = rngState * 1664525u + 1013904223u;
    return rngState;
  };
  n = gridSize();
  const int total = n * n;
  int fillPct = 45;
  if (difficulty == Difficulty::Easy) fillPct = 50;
  if (difficulty == Difficulty::Hard) fillPct = 45;

  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      solution[y * MAX_N + x] = (static_cast<int>(rnd() % 100) < fillPct) ? 1 : 0;
    }

  int ones = 0;
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) ones += solution[y * MAX_N + x];
  if (ones < std::max(4, n)) {
    for (int i = 0; i < n; ++i) solution[i * MAX_N + i] = 1;
  }
  if (ones > total - n) {
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) solution[y * MAX_N + x] = ((x + y) % 2 == 0) ? 1 : 0;
  }
}

void NonogramsActivity::continueGame() {
  touchPainting = false;
  progressDirty = false;
  lastPaintX = -1;
  lastPaintY = -1;
  screen = Screen::Playing;
  needsRedraw = true;
  requestUpdate();
}

void NonogramsActivity::saveProgress() const {
  std::array<NonogramsProgressStore::Mark, NonogramsProgressStore::MAX_N * NonogramsProgressStore::MAX_N> storeGrid{};
  for (int i = 0; i < MAX_N * MAX_N; ++i) {
    storeGrid[i] = static_cast<NonogramsProgressStore::Mark>(grid[i]);
  }
  NonogramsProgressStore::getInstance().saveGame(n, static_cast<NonogramsProgressStore::Difficulty>(difficulty),
                                                 solution, storeGrid, cursorX, cursorY, win);
}

void NonogramsActivity::reset() {
  generatePuzzle();
  std::fill(grid.begin(), grid.end(), Mark::Empty);
  cursorX = 0;
  cursorY = 0;
  win = false;
  paintMode = PaintMode::Fill;
  touchPainting = false;
  progressDirty = false;
  lastPaintX = -1;
  lastPaintY = -1;
  hasSavedGame = true;
  saveProgress();
  needsRedraw = true;
  requestUpdate();
}

bool NonogramsActivity::solved() const {
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const bool filled = (grid[y * MAX_N + x] == Mark::Filled);
      if (filled != (solution[y * MAX_N + x] == 1)) return false;
    }
  return true;
}

NonogramsActivity::BoardLayout NonogramsActivity::computeBoardLayout() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int clueFont = SMALL_FONT_ID;
  const int clueLineH = renderer.getLineHeight(clueFont);

  int maxColClues = 1;
  for (int x = 0; x < n; ++x) {
    int count = 0, run = 0;
    for (int y = 0; y < n; ++y) {
      if (solution[y * MAX_N + x])
        run++;
      else if (run) {
        count++;
        run = 0;
      }
    }
    if (run) count++;
    if (count == 0) count = 1;
    maxColClues = std::max(maxColClues, count);
  }

  const int leftClueW = (n <= 5) ? 36 : (n <= 10 ? 48 : 56);
  const int topClueH = maxColClues * clueLineH + 4;
  const int gridTop = metrics.topPadding + metrics.headerHeight + topClueH + 6;
  const int gridLeft = leftClueW;
  const int toggleHeight = std::max(44, renderer.getLineHeight(UI_10_FONT_ID) + 24);
  const int toggleGap = 14;
  const int availW = renderer.getScreenWidth() - gridLeft - 6;
  const int availH = renderer.getScreenHeight() - gridTop - metrics.buttonHintsHeight - 6 - toggleHeight - toggleGap;
  const int cell = std::max(10, std::min(availW / n, availH / n));
  const int toggleTop = gridTop + n * cell + toggleGap;

  BoardLayout layout;
  layout.gridLeft = gridLeft;
  layout.gridTop = gridTop;
  layout.cell = cell;
  layout.toggleTop = toggleTop;
  layout.toggleHeight = toggleHeight;
  return layout;
}

bool NonogramsActivity::gridCellAt(int px, int py, int& outX, int& outY, const BoardLayout& layout) const {
  if (px >= layout.gridLeft && px < layout.gridLeft + layout.cell * n && py >= layout.gridTop &&
      py < layout.gridTop + layout.cell * n) {
    outX = (px - layout.gridLeft) / layout.cell;
    outY = (py - layout.gridTop) / layout.cell;
    return true;
  }
  return false;
}

bool NonogramsActivity::toggleButtonHit(int px, int py, const BoardLayout& layout) const {
  const int toggleW = std::max(120, n * layout.cell);
  const int toggleLeft = layout.gridLeft;
  return px >= toggleLeft && px < toggleLeft + toggleW && py >= layout.toggleTop &&
         py < layout.toggleTop + layout.toggleHeight;
}

void NonogramsActivity::applyPaint(int x, int y, bool forceTarget, bool& changed) {
  if (x < 0 || y < 0 || x >= n || y >= n) return;
  auto& m = grid[y * MAX_N + x];
  const Mark target = (paintMode == PaintMode::Fill) ? Mark::Filled : Mark::Cross;
  if (forceTarget) {
    if (m == target) {
      cursorX = x;
      cursorY = y;
      return;
    }
    m = target;
  } else {
    m = (m == target) ? Mark::Empty : target;
  }
  cursorX = x;
  cursorY = y;
  win = solved();
  changed = true;
}

int NonogramsActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

void NonogramsActivity::loop() {
  if (screen == Screen::StartMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      startSelection = 1 - startSelection;
      needsRedraw = true;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (startSelection == 0) {
        continueGame();
      } else {
        screen = Screen::Difficulty;
        needsRedraw = true;
        requestUpdate();
      }
      return;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
      const int top = startMenuTop();
      const int idx = (ty - top) / rowHeight;
      const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
      if (tx >= margin && tx < renderer.getScreenWidth() - margin && idx >= 0 && idx < 2 && ty >= top) {
        startSelection = idx;
        if (startSelection == 0) {
          continueGame();
        } else {
          screen = Screen::Difficulty;
          needsRedraw = true;
          requestUpdate();
        }
      }
    }
    if (needsRedraw) {
      needsRedraw = false;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Playing) {
      saveProgress();
      touchPainting = false;
      progressDirty = false;
      lastPaintX = -1;
      lastPaintY = -1;
      screen = Screen::Difficulty;
      needsRedraw = true;
      requestUpdate();
      return;
    }
    if (hasSavedGame) {
      screen = Screen::StartMenu;
      needsRedraw = true;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (screen == Screen::Difficulty) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = static_cast<Difficulty>((static_cast<int>(difficulty) + 2) % 3);
      needsRedraw = true;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = static_cast<Difficulty>((static_cast<int>(difficulty) + 1) % 3);
      needsRedraw = true;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      screen = Screen::Playing;
      reset();
      return;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
      const int top = metrics.topPadding + metrics.headerHeight + 50;
      const int idx = (ty - top) / rowH;
      if (idx >= 0 && idx < 3) {
        difficulty = static_cast<Difficulty>(idx);
        screen = Screen::Playing;
        reset();
      }
    }
    if (needsRedraw) {
      needsRedraw = false;
      requestUpdate();
    }
    return;
  }

  if (win) {
    touchPainting = false;
    progressDirty = false;
    lastPaintX = -1;
    lastPaintY = -1;
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      reset();
      return;
    }
    if (needsRedraw) {
      needsRedraw = false;
      requestUpdate();
    }
    return;
  }

  const BoardLayout layout = computeBoardLayout();
  bool changed = false;

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorX = std::max(0, cursorX - 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorX = std::min(n - 1, cursorX + 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    cursorY = std::max(0, cursorY - 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    cursorY = std::min(n - 1, cursorY + 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    applyPaint(cursorX, cursorY, false, changed);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
             mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    paintMode = (paintMode == PaintMode::Fill) ? PaintMode::Cross : PaintMode::Fill;
    changed = true;
  }

  bool longPressHandled = false;
  int lx = 0, ly = 0;
  if (mappedInput.wasScreenLongPress(lx, ly)) {
    int gx = 0, gy = 0;
    if (gridCellAt(lx, ly, gx, gy, layout)) {
      auto& m = grid[gy * MAX_N + gx];
      m = (m == Mark::Cross) ? Mark::Empty : Mark::Cross;
      cursorX = gx;
      cursorY = gy;
      win = solved();
      changed = true;
      longPressHandled = true;
      touchPainting = false;
      progressDirty = false;
      lastPaintX = -1;
      lastPaintY = -1;
    }
  }

  bool shouldSave = false;
  if (!longPressHandled) {
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      if (toggleButtonHit(tx, ty, layout)) {
        paintMode = (paintMode == PaintMode::Fill) ? PaintMode::Cross : PaintMode::Fill;
        changed = true;
      } else {
        int gx = 0, gy = 0;
        if (gridCellAt(tx, ty, gx, gy, layout)) {
          if (!(touchPainting && gx == lastPaintX && gy == lastPaintY)) {
            applyPaint(gx, gy, false, changed);
          }
        }
      }
      if (changed) shouldSave = true;
    }

    int hx = 0, hy = 0;
    const bool held = mappedInput.isScreenTouchHeld(hx, hy);
    if (held) {
      int gx = 0, gy = 0;
      if (gridCellAt(hx, hy, gx, gy, layout)) {
        if (!touchPainting || gx != lastPaintX || gy != lastPaintY) {
          applyPaint(gx, gy, true, changed);
          touchPainting = true;
          lastPaintX = gx;
          lastPaintY = gy;
        }
        if (changed) progressDirty = true;
      } else if (touchPainting) {
        lastPaintX = -1;
        lastPaintY = -1;
      }
    } else {
      if (progressDirty) shouldSave = true;
      touchPainting = false;
      progressDirty = false;
      lastPaintX = -1;
      lastPaintY = -1;
    }
  } else if (changed) {
    shouldSave = true;
  }

  // Non-touch button/cursor changes are discrete; save immediately when changed and no drag-save is pending.
  if (changed && !progressDirty && !shouldSave) {
    shouldSave = true;
  }
  if (shouldSave) saveProgress();
  if (changed || needsRedraw) {
    needsRedraw = false;
    requestUpdate();
  }
}

void NonogramsActivity::drawStartMenu() const {
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int top = startMenuTop();
  const char* labels[] = {tr(STR_CONTINUE), tr(STR_NEW_GAME)};
  const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
  for (int i = 0; i < 2; ++i) {
    const int y = top + i * rowHeight;
    const bool selected = startSelection == i;
    if (selected) {
      renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    } else {
      renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, labels[i],
                              !selected);
  }
}

void NonogramsActivity::drawDifficulty() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int top = metrics.topPadding + metrics.headerHeight + 50;
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, tr(STR_SUDOKU_SELECT_DIFFICULTY));
  const char* opts[] = {tr(STR_SUDOKU_EASY), tr(STR_SUDOKU_NORMAL), tr(STR_SUDOKU_HARD)};
  const char* sizes[] = {"5 x 5", "10 x 10", "15 x 15"};
  for (int i = 0; i < 3; ++i) {
    const bool sel = (i == static_cast<int>(difficulty));
    const int y = top + i * rowH;
    if (sel)
      renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else
      renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    char label[32];
    snprintf(label, sizeof(label), "%s (%s)", opts[i], sizes[i]);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label, !sel);
  }
}

void NonogramsActivity::drawBoard() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int clueFont = SMALL_FONT_ID;
  const int clueLineH = renderer.getLineHeight(clueFont);
  const BoardLayout layout = computeBoardLayout();

  // build clues
  std::vector<std::vector<int>> rowClues(n), colClues(n);
  for (int y = 0; y < n; ++y) {
    int run = 0;
    for (int x = 0; x < n; ++x) {
      if (solution[y * MAX_N + x])
        run++;
      else if (run) {
        rowClues[y].push_back(run);
        run = 0;
      }
    }
    if (run) rowClues[y].push_back(run);
    if (rowClues[y].empty()) rowClues[y].push_back(0);
  }
  for (int x = 0; x < n; ++x) {
    int run = 0;
    for (int y = 0; y < n; ++y) {
      if (solution[y * MAX_N + x])
        run++;
      else if (run) {
        colClues[x].push_back(run);
        run = 0;
      }
    }
    if (run) colClues[x].push_back(run);
    if (colClues[x].empty()) colClues[x].push_back(0);
  }

  const int gridTop = layout.gridTop;
  const int gridLeft = layout.gridLeft;
  const int cell = layout.cell;

  // row clues (left)
  for (int y = 0; y < n; ++y) {
    std::string s;
    for (size_t i = 0; i < rowClues[y].size(); ++i) {
      if (i) s += ' ';
      s += std::to_string(rowClues[y][i]);
    }
    const int tw = renderer.getTextWidth(clueFont, s.c_str());
    const int tx = std::max(2, gridLeft - 4 - tw);
    const int ty = gridTop + y * cell + (cell - clueLineH) / 2;
    renderer.drawText(clueFont, tx, ty, s.c_str());
  }

  // column clues (top) - stacked vertically per column, bottom-aligned in clue area
  const int clueAreaBottom = gridTop - 2;
  for (int x = 0; x < n; ++x) {
    const int cx = gridLeft + x * cell;
    for (int i = static_cast<int>(colClues[x].size()) - 1, row = 0; i >= 0; --i, ++row) {
      const std::string s = std::to_string(colClues[x][i]);
      const int tw = renderer.getTextWidth(clueFont, s.c_str());
      const int tx = cx + (cell - tw) / 2;
      const int ty = clueAreaBottom - (row + 1) * clueLineH;
      renderer.drawText(clueFont, tx, ty, s.c_str());
    }
  }

  // grid
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const int px = gridLeft + x * cell;
      const int py = gridTop + y * cell;
      renderer.drawRect(px, py, cell, cell);
      const Mark m = grid[y * MAX_N + x];
      if (m == Mark::Filled) {
        renderer.fillRect(px + 2, py + 2, std::max(1, cell - 4), std::max(1, cell - 4));
      } else if (m == Mark::Cross) {
        renderer.drawLine(px + 3, py + 3, px + cell - 3, py + cell - 3);
        renderer.drawLine(px + cell - 3, py + 3, px + 3, py + cell - 3);
      }
      if (x == cursorX && y == cursorY) renderer.drawRect(px, py, cell, cell, 3, true);
    }

  // thicker 5-cell guides
  for (int i = 5; i < n; i += 5) {
    renderer.drawLine(gridLeft + i * cell, gridTop, gridLeft + i * cell, gridTop + n * cell, 2);
    renderer.drawLine(gridLeft, gridTop + i * cell, gridLeft + n * cell, gridTop + i * cell, 2);
  }
  renderer.drawRect(gridLeft, gridTop, n * cell, n * cell, 2, true);

  // paint mode toggle
  const int toggleW = std::max(120, n * cell);
  renderer.drawRect(gridLeft, layout.toggleTop, toggleW, layout.toggleHeight);
  const char* modeLabel =
      (paintMode == PaintMode::Fill) ? tr(STR_NONOGRAMS_MODE_FILL_LABEL) : tr(STR_NONOGRAMS_MODE_MARK_LABEL);
  renderer.drawCenteredText(
      SMALL_FONT_ID, layout.toggleTop + std::max(0, (layout.toggleHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2),
      modeLabel);

  if (win)
    renderer.drawCenteredText(UI_12_FONT_ID, layout.toggleTop + layout.toggleHeight + 4, tr(STR_SUDOKU_COMPLETE));
}

void NonogramsActivity::render(RenderLock&&) {
  needsRedraw = false;
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_NONOGRAMS));
  if (screen == Screen::StartMenu)
    drawStartMenu();
  else if (screen == Screen::Difficulty)
    drawDifficulty();
  else
    drawBoard();

  const char* hint = tr(STR_SELECT);
  if (screen == Screen::Playing && win)
    hint = tr(STR_NEW_GAME);
  else if (screen == Screen::Playing)
    hint = (paintMode == PaintMode::Fill) ? tr(STR_NONOGRAMS_FILL) : tr(STR_NONOGRAMS_MARK);
  const char* leftHint = tr(STR_DIR_UP);
  const char* rightHint = tr(STR_DIR_DOWN);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), hint, leftHint, rightHint);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
