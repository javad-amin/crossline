#include "MinesweeperActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "MinesweeperProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* difficultyName(const minesweeper::Difficulty d) {
  switch (d) {
    case minesweeper::Difficulty::Easy: return tr(STR_SUDOKU_EASY); // reuse Easy/Normal/Hard/Expert labels
    case minesweeper::Difficulty::Normal: return tr(STR_SUDOKU_NORMAL);
    case minesweeper::Difficulty::Hard: return tr(STR_SUDOKU_HARD);
    case minesweeper::Difficulty::Expert: return tr(STR_SUDOKU_EXPERT);
  }
  return tr(STR_SUDOKU_EASY);
}
}  // namespace

MinesweeperActivity::MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Minesweeper", renderer, mappedInput) {}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis());
  if (rngState == 0) rngState = 1;
  auto& progress = MinesweeperProgressStore::getInstance();
  if (progress.loadFromFile() && progress.hasSavedGame()) {
    hasSavedGame = true;
    board = progress.board();
    cursorX = progress.cursorX();
    cursorY = progress.cursorY();
    difficulty = progress.difficulty();
    screen = Screen::StartMenu;
    startSelection = 0;
  } else {
    screen = Screen::Difficulty;
  }
  requestUpdate();
}

void MinesweeperActivity::newGame() {
  board.cfg = minesweeper::configFor(difficulty);
  board.mine.fill(0);
  board.revealed.fill(0);
  board.flagged.fill(0);
  board.adjacent.fill(0);
  board.generated = false;
  board.exploded = false;
  board.won = false;
  board.revealedCount = 0;
  cursorX = board.cfg.w / 2;
  cursorY = board.cfg.h / 2;
  screen = Screen::Playing;
  hasSavedGame = true;
  saveProgress();
  requestUpdate();
}

void MinesweeperActivity::continueGame() {
  screen = (board.exploded || board.won) ? Screen::Result : Screen::Playing;
  requestUpdate();
}

void MinesweeperActivity::saveProgress() const {
  MinesweeperProgressStore::getInstance().saveGame(board, cursorX, cursorY, difficulty);
}

int MinesweeperActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

int MinesweeperActivity::cellSize() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + 28;
  const int bottom = metrics.buttonHintsHeight + 8;
  const int availW = renderer.getScreenWidth() - 16;
  const int availH = renderer.getScreenHeight() - top - bottom;
  const int csW = std::max(14, availW / board.cfg.w);
  const int csH = std::max(14, availH / board.cfg.h);
  return std::min(csW, csH);
}

int MinesweeperActivity::boardLeft() const {
  return (renderer.getScreenWidth() - cellSize() * board.cfg.w) / 2;
}

int MinesweeperActivity::boardTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + 28;
  const int bottom = metrics.buttonHintsHeight + 8;
  const int availH = renderer.getScreenHeight() - top - bottom;
  return top + std::max(0, (availH - cellSize() * board.cfg.h) / 2);
}

void MinesweeperActivity::loop() {
  if (screen == Screen::StartMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      startSelection = 1 - startSelection;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (startSelection == 0) continueGame();
      else {
        screen = Screen::Difficulty;
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
        if (startSelection == 0) continueGame();
        else {
          screen = Screen::Difficulty;
          requestUpdate();
        }
      }
    }
    return;
  }

  if (screen == Screen::Difficulty) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (hasSavedGame) {
        screen = Screen::StartMenu;
        requestUpdate();
      } else {
        finish();
      }
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = static_cast<minesweeper::Difficulty>((static_cast<int>(difficulty) + 3) % 4);
      requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = static_cast<minesweeper::Difficulty>((static_cast<int>(difficulty) + 1) % 4);
      requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { newGame(); return; }
    int x=0,y=0;
    if (mappedInput.wasScreenTapped(x,y)) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int rowHeight = std::max(42, renderer.getLineHeight(UI_12_FONT_ID) + 20);
      const int listTop = metrics.topPadding + metrics.headerHeight + 40;
      const int idx = (y - listTop) / rowHeight;
      if (idx >= 0 && idx < 4) { difficulty = static_cast<minesweeper::Difficulty>(idx); newGame(); }
    }
    return;
  }

  if (screen == Screen::Result) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { screen = Screen::Difficulty; requestUpdate(); return; }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { newGame(); return; }
    int x=0,y=0; if (mappedInput.wasScreenTapped(x,y)) newGame();
    return;
  }

  // Playing
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { saveProgress(); screen = Screen::Difficulty; requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { cursorY = std::max(0, cursorY - 1); saveProgress(); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { cursorY = std::min(board.cfg.h - 1, cursorY + 1); saveProgress(); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) { cursorX = std::max(0, cursorX - 1); saveProgress(); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) { cursorX = std::min(board.cfg.w - 1, cursorX + 1); saveProgress(); requestUpdate(); return; }

  // Long press confirm (if supported) to flag, short press to reveal
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, 600)) {
    minesweeper::toggleFlag(board, cursorX, cursorY);
    saveProgress();
    requestUpdate(); return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const int i = board.idx(cursorX, cursorY);
    if (board.revealed[i]) {
      // chord-like: reveal neighbors if flags match (simple helper behavior)
      // keep simple: toggle flag on revealed? no, ignore
    } else {
      minesweeper::reveal(board, rngState, cursorX, cursorY);
    }
    if (board.exploded || board.won) screen = Screen::Result;
    saveProgress();
    requestUpdate();
    return;
  }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenLongPress(tx, ty)) {
    const int cs = cellSize();
    const int bl = boardLeft(), bt = boardTop();
    if (tx >= bl && tx < bl + cs * board.cfg.w && ty >= bt && ty < bt + cs * board.cfg.h) {
      cursorX = (tx - bl) / cs;
      cursorY = (ty - bt) / cs;
      minesweeper::toggleFlag(board, cursorX, cursorY);
      saveProgress();
      requestUpdate();
      return;
    }
  }
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const int cs = cellSize();
    const int bl = boardLeft(), bt = boardTop();
    if (tx >= bl && tx < bl + cs * board.cfg.w && ty >= bt && ty < bt + cs * board.cfg.h) {
      cursorX = (tx - bl) / cs;
      cursorY = (ty - bt) / cs;
      minesweeper::reveal(board, rngState, cursorX, cursorY);
      if (board.exploded || board.won) screen = Screen::Result;
      saveProgress();
      requestUpdate();
    }
  }
}

void MinesweeperActivity::drawStartMenu() {
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

void MinesweeperActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_MINESWEEPER));

  if (screen == Screen::StartMenu) drawStartMenu();
  else if (screen == Screen::Difficulty) drawDifficulty();
  else {
    drawBoard();
    if (screen == Screen::Result) drawResult();
  }
  drawFooter();
  renderer.displayBuffer();
}

void MinesweeperActivity::drawDifficulty() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(42, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int listTop = metrics.topPadding + metrics.headerHeight + 40;
  renderer.drawCenteredText(UI_12_FONT_ID, listTop - renderer.getLineHeight(UI_12_FONT_ID) - 12, tr(STR_MINESWEEPER_SELECT_DIFFICULTY));
  const minesweeper::Difficulty opts[] = {minesweeper::Difficulty::Easy, minesweeper::Difficulty::Normal, minesweeper::Difficulty::Hard, minesweeper::Difficulty::Expert};
  const int margin = std::max(20, metrics.contentSidePadding);
  for (int i = 0; i < 4; ++i) {
    const int y = listTop + i * rowHeight;
    const bool selected = opts[i] == difficulty;
    if (selected) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    const auto cfg = minesweeper::configFor(opts[i]);
    char label[48];
    snprintf(label, sizeof(label), "%s (%dx%d, %d)", difficultyName(opts[i]), cfg.w, cfg.h, cfg.mines);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, (renderer.getScreenWidth() - tw) / 2,
                      y + (rowHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label, !selected);
  }
}

void MinesweeperActivity::drawBoard() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  if (screen != Screen::Result) {
    char status[64];
    snprintf(status, sizeof(status), "%s: %d   %s: %d/%d", tr(STR_MINESWEEPER_MINES), board.cfg.mines,
             tr(STR_MINESWEEPER_FLAGS), minesweeper::flaggedCount(board), board.cfg.mines);
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 4, status);
  }

  const int cs = cellSize();
  const int bl = boardLeft();
  const int bt = boardTop();
  const int bw = cs * board.cfg.w;
  const int bh = cs * board.cfg.h;
  renderer.drawRect(bl - 2, bt - 2, bw + 4, bh + 4, 2, true);

  for (int y = 0; y < board.cfg.h; ++y) {
    for (int x = 0; x < board.cfg.w; ++x) {
      const int i = board.idx(x, y);
      const int px = bl + x * cs;
      const int py = bt + y * cs;
      const bool cursor = (x == cursorX && y == cursorY);
      const bool revealed = board.revealed[i] != 0;
      const bool mine = board.mine[i] != 0;
      const bool flagged = board.flagged[i] != 0;

      if (revealed) {
        if (mine) {
          renderer.fillRect(px + 1, py + 1, cs - 1, cs - 1);
        } else {
          renderer.fillRect(px + 1, py + 1, cs - 1, cs - 1, false);
        }
      } else {
        renderer.fillRectDither(px + 1, py + 1, cs - 1, cs - 1, Color::LightGray);
      }
      renderer.drawRect(px, py, cs, cs, true);

      const int cx = px + cs / 2;
      const int cy = py + cs / 2;

      if (revealed && mine) {
        const int r = std::max(2, cs / 6);
        renderer.fillRect(cx - r, cy - r / 2, r * 2, r, false);
        renderer.fillRect(cx - r / 2, cy - r, r, r * 2, false);
        renderer.drawPixel(cx, cy, false);
      } else if (!revealed && flagged) {
        const int poleX = cx - 1;
        renderer.fillRect(poleX, cy - cs / 4, 2, cs / 2, true);
        renderer.fillRect(poleX + 2, cy - cs / 4, std::max(3, cs / 4), std::max(3, cs / 5), true);
        renderer.fillRect(cx - 3, cy + cs / 6, 6, 2, true);
      } else if (revealed && !mine && board.adjacent[i] > 0) {
        char num[2] = {static_cast<char>('0' + board.adjacent[i]), '\0'};
        const int font = (cs >= 28) ? UI_10_FONT_ID : SMALL_FONT_ID;
        const int tw = renderer.getTextWidth(font, num);
        const int th = renderer.getLineHeight(font);
        renderer.drawText(font, px + (cs - tw) / 2, py + (cs - th) / 2, num);
      }

      if (cursor) {
        renderer.drawRect(px + 1, py + 1, cs - 2, cs - 2, 2, true);
      }
    }
  }
}

void MinesweeperActivity::drawResult() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int y = metrics.topPadding + metrics.headerHeight + 4;
  if (board.won) renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_MINESWEEPER_WIN));
  else if (board.exploded) renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_MINESWEEPER_LOSE));
}

void MinesweeperActivity::drawFooter() {
  if (screen == Screen::Playing) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MINESWEEPER_REVEAL_FLAG), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    return;
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
