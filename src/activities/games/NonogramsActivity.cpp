#include "NonogramsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

NonogramsActivity::NonogramsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Nonograms", renderer, mappedInput) {}

void NonogramsActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis());
  if (rngState == 0) rngState = 1;
  screen = Screen::Difficulty;
  difficulty = Difficulty::Normal;
  needsRedraw = true;
  requestUpdate();
}

int NonogramsActivity::gridSize() const {
  switch (difficulty) {
    case Difficulty::Easy: return 5;
    case Difficulty::Normal: return 10;
    case Difficulty::Hard: return 15;
  }
  return 10;
}

void NonogramsActivity::generatePuzzle() {
  auto rnd = [&]() -> uint32_t { rngState = rngState * 1664525u + 1013904223u; return rngState; };
  n = gridSize();
  const int total = n * n;
  int fillPct = 45;
  if (difficulty == Difficulty::Easy) fillPct = 50;
  if (difficulty == Difficulty::Hard) fillPct = 45;

  for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
    solution[y * MAX_N + x] = (static_cast<int>(rnd() % 100) < fillPct) ? 1 : 0;
  }

  int ones = 0;
  for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) ones += solution[y * MAX_N + x];
  if (ones < std::max(4, n)) {
    for (int i = 0; i < n; ++i) solution[i * MAX_N + i] = 1;
  }
  if (ones > total - n) {
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) solution[y * MAX_N + x] = ((x + y) % 2 == 0) ? 1 : 0;
  }
}

void NonogramsActivity::reset() {
  generatePuzzle();
  std::fill(grid.begin(), grid.end(), Mark::Empty);
  cursorX = 0;
  cursorY = 0;
  win = false;
  needsRedraw = true;
  requestUpdate();
}

bool NonogramsActivity::solved() const {
  for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
    const bool filled = (grid[y * MAX_N + x] == Mark::Filled);
    if (filled != (solution[y * MAX_N + x] == 1)) return false;
  }
  return true;
}

void NonogramsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Playing) {
      screen = Screen::Difficulty;
      needsRedraw = true;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (screen == Screen::Difficulty) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = static_cast<Difficulty>((static_cast<int>(difficulty) + 2) % 3);
      needsRedraw = true; requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = static_cast<Difficulty>((static_cast<int>(difficulty) + 1) % 3);
      needsRedraw = true; requestUpdate(); return;
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
    if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  if (win) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { reset(); return; }
    if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  bool changed = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) { cursorX = std::max(0, cursorX - 1); changed = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) { cursorX = std::min(n - 1, cursorX + 1); changed = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { cursorY = std::max(0, cursorY - 1); changed = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { cursorY = std::min(n - 1, cursorY + 1); changed = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    auto& m = grid[cursorY * MAX_N + cursorX];
    m = (m == Mark::Filled) ? Mark::Empty : Mark::Filled;
    if (solved()) win = true;
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) || mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    auto& m = grid[cursorY * MAX_N + cursorX];
    m = (m == Mark::Cross) ? Mark::Empty : Mark::Cross;
    changed = true;
  }

  auto gridCellAt = [&](int px, int py, int& outX, int& outY) -> bool {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int clueFont = SMALL_FONT_ID;
    const int clueLineH = renderer.getLineHeight(clueFont);

    int maxColClues = 1;
    for (int x = 0; x < n; ++x) {
      int count = 0, run = 0;
      for (int y = 0; y < n; ++y) {
        if (solution[y * MAX_N + x]) run++; else if (run) { count++; run = 0; }
      }
      if (run) count++;
      if (count == 0) count = 1;
      maxColClues = std::max(maxColClues, count);
    }

    const int leftClueW = (n <= 5) ? 36 : (n <= 10 ? 48 : 56);
    const int topClueH = maxColClues * clueLineH + 4;
    const int top = metrics.topPadding + metrics.headerHeight + topClueH + 6;
    const int left = leftClueW;
    const int availW = renderer.getScreenWidth() - left - 6;
    const int availH = renderer.getScreenHeight() - top - metrics.buttonHintsHeight - 6;
    const int cell = std::max(10, std::min(availW / n, availH / n));
    if (px >= left && px < left + cell * n && py >= top && py < top + cell * n) {
      outX = (px - left) / cell;
      outY = (py - top) / cell;
      return true;
    }
    return false;
  };

  int lx = 0, ly = 0;
  if (mappedInput.wasScreenLongPress(lx, ly)) {
    int gx = 0, gy = 0;
    if (gridCellAt(lx, ly, gx, gy)) {
      cursorX = gx;
      cursorY = gy;
      auto& m = grid[cursorY * MAX_N + cursorX];
      m = (m == Mark::Cross) ? Mark::Empty : Mark::Cross;
      changed = true;
    }
  } else {
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      int gx = 0, gy = 0;
      if (gridCellAt(tx, ty, gx, gy)) {
        cursorX = gx;
        cursorY = gy;
        auto& m = grid[cursorY * MAX_N + cursorX];
        m = (m == Mark::Filled) ? Mark::Empty : Mark::Filled;
        if (solved()) win = true;
        changed = true;
      }
    }
  }

  if (changed || needsRedraw) { needsRedraw = false; requestUpdate(); }
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
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    char label[32];
    snprintf(label, sizeof(label), "%s (%s)", opts[i], sizes[i]);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label, !sel);
  }
}

void NonogramsActivity::drawBoard() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int clueFont = SMALL_FONT_ID;
  const int clueLineH = renderer.getLineHeight(clueFont);

  // build clues
  std::vector<std::vector<int>> rowClues(n), colClues(n);
  for (int y = 0; y < n; ++y) {
    int run = 0;
    for (int x = 0; x < n; ++x) {
      if (solution[y * MAX_N + x]) run++; else if (run) { rowClues[y].push_back(run); run = 0; }
    }
    if (run) rowClues[y].push_back(run);
    if (rowClues[y].empty()) rowClues[y].push_back(0);
  }
  for (int x = 0; x < n; ++x) {
    int run = 0;
    for (int y = 0; y < n; ++y) {
      if (solution[y * MAX_N + x]) run++; else if (run) { colClues[x].push_back(run); run = 0; }
    }
    if (run) colClues[x].push_back(run);
    if (colClues[x].empty()) colClues[x].push_back(0);
  }

  int maxColClues = 1;
  for (int x = 0; x < n; ++x) maxColClues = std::max(maxColClues, static_cast<int>(colClues[x].size()));

  const int leftClueW = (n <= 5) ? 36 : (n <= 10 ? 48 : 56);
  const int topClueH = maxColClues * clueLineH + 4;
  const int gridTop = metrics.topPadding + metrics.headerHeight + topClueH + 6;
  const int gridLeft = leftClueW;
  const int availW = renderer.getScreenWidth() - gridLeft - 6;
  const int availH = renderer.getScreenHeight() - gridTop - metrics.buttonHintsHeight - 6;
  const int cell = std::max(10, std::min(availW / n, availH / n));

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
  for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
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

  if (win) renderer.drawCenteredText(UI_12_FONT_ID, gridTop + n * cell + 4, tr(STR_SUDOKU_COMPLETE));
}

void NonogramsActivity::render(RenderLock&&) {
  needsRedraw = false;
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_NONOGRAMS));
  if (screen == Screen::Difficulty) drawDifficulty();
  else drawBoard();

  const char* hint = tr(STR_SELECT);
  if (screen == Screen::Playing && win) hint = tr(STR_NEW_GAME);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), hint, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
