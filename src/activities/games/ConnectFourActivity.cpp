#include "ConnectFourActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <limits>

#include "ConnectFourProgressStore.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

ConnectFourActivity::ConnectFourActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("ConnectFour", renderer, mappedInput) {}

void ConnectFourActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis());
  if (rngState == 0) rngState = 1;
  auto& progress = ConnectFourProgressStore::getInstance();
  if (progress.loadFromFile() && progress.hasSavedGame()) {
    hasSavedGame = true;
    screen = static_cast<Screen>(progress.screen());
    for (int i = 0; i < COLS * ROWS; ++i) {
      board[i] = static_cast<Cell>(progress.board()[i]);
    }
    cursorCol = progress.cursorCol();
    state = static_cast<State>(progress.state());
    mode = static_cast<Mode>(progress.mode());
    difficulty = static_cast<Difficulty>(progress.difficulty());
    current = static_cast<Cell>(progress.current());
    rngState = progress.rngState();
    screen = Screen::StartMenu;
    startSelection = 0;
  } else {
    screen = Screen::ModeSelect;
    mode = Mode::VsAI;
    difficulty = Difficulty::Normal;
    resetBoard();
  }
  needsRedraw = true;
  requestUpdate();
}

void ConnectFourActivity::resetBoard() {
  board.fill(Cell::Empty);
  cursorCol = COLS / 2;
  state = State::Playing;
  current = Cell::P1;
  aiThinking = false;
  needsRedraw = true;
}

void ConnectFourActivity::continueGame() {
  auto& progress = ConnectFourProgressStore::getInstance();
  screen = static_cast<Screen>(progress.screen());
  needsRedraw = true;
  requestUpdate();
}

void ConnectFourActivity::saveProgress() const {
  std::array<ConnectFourProgressStore::Cell, ConnectFourProgressStore::COLS * ConnectFourProgressStore::ROWS> storeBoard{};
  for (int i = 0; i < COLS * ROWS; ++i) {
    storeBoard[i] = static_cast<ConnectFourProgressStore::Cell>(board[i]);
  }
  ConnectFourProgressStore::getInstance().saveGame(
      static_cast<ConnectFourProgressStore::Screen>(screen), storeBoard, cursorCol,
      static_cast<ConnectFourProgressStore::State>(state), static_cast<ConnectFourProgressStore::Mode>(mode),
      static_cast<ConnectFourProgressStore::Difficulty>(difficulty),
      static_cast<ConnectFourProgressStore::Cell>(current), rngState);
}

int ConnectFourActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

bool ConnectFourActivity::dropInBoard(std::array<Cell, COLS * ROWS>& b, int col, Cell who) const {
  if (col < 0 || col >= COLS) return false;
  for (int r = ROWS - 1; r >= 0; --r) {
    const int idx = r * COLS + col;
    if (b[idx] == Cell::Empty) {
      b[idx] = who;
      return true;
    }
  }
  return false;
}

bool ConnectFourActivity::dropInColumn(int col, Cell who) { return dropInBoard(board, col, who); }

bool ConnectFourActivity::isFull(const std::array<Cell, COLS * ROWS>& b) const {
  for (int c = 0; c < COLS; ++c) {
    if (b[c] == Cell::Empty) return false;
  }
  return true;
}

bool ConnectFourActivity::checkWin(const std::array<Cell, COLS * ROWS>& b, Cell who) const {
  const auto at = [&](int c, int r) -> Cell {
    if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return Cell::Empty;
    return b[r * COLS + c];
  };
  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS; ++c) {
      if (at(c, r) != who) continue;
      if (at(c + 1, r) == who && at(c + 2, r) == who && at(c + 3, r) == who) return true;
      if (at(c, r + 1) == who && at(c, r + 2) == who && at(c, r + 3) == who) return true;
      if (at(c + 1, r + 1) == who && at(c + 2, r + 2) == who && at(c + 3, r + 3) == who) return true;
      if (at(c + 1, r - 1) == who && at(c + 2, r - 2) == who && at(c + 3, r - 3) == who) return true;
    }
  }
  return false;
}

int ConnectFourActivity::scoreBoard(const std::array<Cell, COLS * ROWS>& b, Cell who) const {
  const Cell opp = (who == Cell::P2) ? Cell::P1 : Cell::P2;
  auto scoreWindow = [&](int c, int r, int dc, int dr) -> int {
    int mine = 0, theirs = 0, empty = 0;
    for (int i = 0; i < 4; ++i) {
      const int x = c + dc * i;
      const int y = r + dr * i;
      if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return 0;
      const Cell v = b[y * COLS + x];
      if (v == who) ++mine;
      else if (v == opp) ++theirs;
      else ++empty;
    }
    if (mine == 4) return 100000;
    if (theirs == 4) return -100000;
    if (mine == 3 && empty == 1) return 200;
    if (mine == 2 && empty == 2) return 20;
    if (theirs == 3 && empty == 1) return -220;  // block urgency
    if (theirs == 2 && empty == 2) return -18;
    return 0;
  };

  int score = 0;
  // center column preference
  const int center = COLS / 2;
  for (int r = 0; r < ROWS; ++r) {
    if (b[r * COLS + center] == who) score += 8;
    else if (b[r * COLS + center] == opp) score -= 8;
  }
  // near-center columns
  for (int r = 0; r < ROWS; ++r) {
    for (int c : {2, 4}) {
      if (b[r * COLS + c] == who) score += 3;
      else if (b[r * COLS + c] == opp) score -= 3;
    }
  }

  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS; ++c) {
      score += scoreWindow(c, r, 1, 0);
      score += scoreWindow(c, r, 0, 1);
      score += scoreWindow(c, r, 1, 1);
      score += scoreWindow(c, r, 1, -1);
    }
  }
  return score;
}

int ConnectFourActivity::minimax(std::array<Cell, COLS * ROWS>& b, int depth, int alpha, int beta, bool maximizing) const {
  if (checkWin(b, Cell::P2)) return 1000000 + depth;
  if (checkWin(b, Cell::P1)) return -1000000 - depth;
  if (isFull(b) || depth == 0) return scoreBoard(b, Cell::P2);

  const int order[] = {3, 2, 4, 1, 5, 0, 6};
  if (maximizing) {
    int best = std::numeric_limits<int>::min();
    for (int c : order) {
      if (b[c] != Cell::Empty) continue;
      auto copy = b;
      if (!dropInBoard(copy, c, Cell::P2)) continue;
      const int val = minimax(copy, depth - 1, alpha, beta, false);
      best = std::max(best, val);
      alpha = std::max(alpha, val);
      if (alpha >= beta) break;
    }
    return best == std::numeric_limits<int>::min() ? 0 : best;
  } else {
    int best = std::numeric_limits<int>::max();
    for (int c : order) {
      if (b[c] != Cell::Empty) continue;
      auto copy = b;
      if (!dropInBoard(copy, c, Cell::P1)) continue;
      const int val = minimax(copy, depth - 1, alpha, beta, true);
      best = std::min(best, val);
      beta = std::min(beta, val);
      if (alpha >= beta) break;
    }
    return best == std::numeric_limits<int>::max() ? 0 : best;
  }
}

int ConnectFourActivity::aiMove() {
  auto canDrop = [&](int c) { return c >= 0 && c < COLS && board[c] == Cell::Empty; };
  auto rnd = [&]() -> uint32_t { rngState = rngState * 1664525u + 1013904223u; return rngState; };

  // immediate win
  for (int c = 0; c < COLS; ++c) {
    if (!canDrop(c)) continue;
    auto copy = board;
    dropInBoard(copy, c, Cell::P2);
    if (checkWin(copy, Cell::P2)) return c;
  }
  // immediate block
  for (int c = 0; c < COLS; ++c) {
    if (!canDrop(c)) continue;
    auto copy = board;
    dropInBoard(copy, c, Cell::P1);
    if (checkWin(copy, Cell::P1)) return c;
  }

  const int order[] = {3, 2, 4, 1, 5, 0, 6};

  if (difficulty == Difficulty::Easy) {
    // basic center preference with randomness and occasional mistakes
    if ((rnd() % 100) < 25) {
      // random valid move
      int choices[COLS], n = 0;
      for (int c : order) if (canDrop(c)) choices[n++] = c;
      if (n > 0) return choices[rnd() % n];
    }
    for (int c : order) if (canDrop(c)) return c;
    return -1;
  }

  if (difficulty == Difficulty::Normal) {
    // shallow minimax lookahead
    const int depth = 4;
    int bestCol = -1;
    int bestScore = std::numeric_limits<int>::min();
    for (int c : order) {
      if (!canDrop(c)) continue;
      auto copy = board;
      dropInBoard(copy, c, Cell::P2);
      int s = minimax(copy, depth - 1, std::numeric_limits<int>::min() / 2, std::numeric_limits<int>::max() / 2, false);
      s += static_cast<int>(rnd() % 4);
      if (s > bestScore) { bestScore = s; bestCol = c; }
    }
    if (bestCol >= 0) return bestCol;
    for (int c : order) if (canDrop(c)) return c;
    return -1;
  }

  // Hard: minimax alpha-beta with deeper lookahead
  const int depth = 7;
  int bestCol = -1;
  int bestScore = std::numeric_limits<int>::min();
  for (int c : order) {
    if (!canDrop(c)) continue;
    auto copy = board;
    dropInBoard(copy, c, Cell::P2);
    int s = minimax(copy, depth - 1, std::numeric_limits<int>::min() / 2, std::numeric_limits<int>::max() / 2, false);
    // tiny jitter to avoid identical games in equal positions
    s += static_cast<int>(rnd() % 2);
    if (s > bestScore) { bestScore = s; bestCol = c; }
  }
  if (bestCol >= 0) return bestCol;
  for (int c : order) if (canDrop(c)) return c;
  return -1;
}

void ConnectFourActivity::loop() {
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
      if (startSelection == 0) continueGame();
      else {
        screen = Screen::ModeSelect;
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
        if (startSelection == 0) continueGame();
        else {
          screen = Screen::ModeSelect;
          needsRedraw = true;
          requestUpdate();
        }
      }
    }
    if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen == Screen::Playing) {
      aiThinking = false;
      saveProgress();
      screen = Screen::ModeSelect;
      needsRedraw = true;
      requestUpdate();
      return;
    }
    if (screen == Screen::DifficultySelect) {
      screen = Screen::ModeSelect;
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

  if (screen == Screen::ModeSelect) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      mode = (mode == Mode::VsAI) ? Mode::PvP : Mode::VsAI;
      needsRedraw = true; requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (mode == Mode::VsAI) screen = Screen::DifficultySelect;
      else { screen = Screen::Playing; resetBoard(); hasSavedGame = true; saveProgress(); }
      needsRedraw = true; requestUpdate(); return;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
      const int top = metrics.topPadding + metrics.headerHeight + 50;
      const int idx = (ty - top) / rowH;
      if (idx == 0 || idx == 1) {
        mode = (idx == 0) ? Mode::VsAI : Mode::PvP;
        if (mode == Mode::VsAI) screen = Screen::DifficultySelect;
        else { screen = Screen::Playing; resetBoard(); hasSavedGame = true; saveProgress(); }
        needsRedraw = true; requestUpdate(); return;
      }
    }
    if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  if (screen == Screen::DifficultySelect) {
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
      resetBoard();
      hasSavedGame = true;
      saveProgress();
      needsRedraw = true; requestUpdate(); return;
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
        resetBoard();
        hasSavedGame = true;
        saveProgress();
        needsRedraw = true; requestUpdate(); return;
      }
    }
    if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  if (state != State::Playing) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      resetBoard();
      hasSavedGame = true;
      saveProgress();
      requestUpdate();
    } else if (needsRedraw) { needsRedraw = false; requestUpdate(); }
    return;
  }

  if (aiThinking) {
    const int aiCol = aiMove();
    if (aiCol >= 0) dropInColumn(aiCol, Cell::P2);
    if (checkWin(Cell::P2)) state = State::P2Win;
    else if (isFull()) state = State::Draw;
    aiThinking = false;
    saveProgress();
    needsRedraw = false;
    requestUpdate();
    return;
  }

  bool changed = false;
  auto playMove = [&](int col) {
    if (!dropInColumn(col, current)) return false;
    if (checkWin(current)) {
      state = (current == Cell::P1) ? State::P1Win : State::P2Win;
      return true;
    }
    if (isFull()) { state = State::Draw; return true; }

    if (mode == Mode::VsAI) {
      aiThinking = true;
      return true;
    } else {
      current = (current == Cell::P1) ? Cell::P2 : Cell::P1;
    }
    return true;
  };

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorCol = std::max(0, cursorCol - 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorCol = std::min(COLS - 1, cursorCol + 1);
    changed = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (playMove(cursorCol)) changed = true;
  }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int top = metrics.topPadding + metrics.headerHeight + 30;
    const int availW = renderer.getScreenWidth() - 24;
    const int availH = renderer.getScreenHeight() - top - metrics.buttonHintsHeight - 16;
    const int cell = std::max(18, std::min(availW / COLS, availH / (ROWS + 1)));
    const int boardW = cell * COLS;
    const int left = (renderer.getScreenWidth() - boardW) / 2;
    if (tx >= left && tx < left + boardW && ty >= top) {
      const int col = (tx - left) / cell;
      cursorCol = std::clamp(col, 0, COLS - 1);
      if (playMove(cursorCol)) changed = true;
    }
  }

  if (changed) saveProgress();
  if (changed || needsRedraw) { needsRedraw = false; requestUpdate(); }
}

void ConnectFourActivity::drawModeSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int top = metrics.topPadding + metrics.headerHeight + 50;
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Choose mode");
  const char* opts[] = {"Play vs Computer", "Player vs Player"};
  for (int i = 0; i < 2; ++i) {
    const bool sel = (i == 0 ? mode == Mode::VsAI : mode == Mode::PvP);
    const int y = top + i * rowH;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, opts[i], !sel);
  }
}

void ConnectFourActivity::drawDifficultySelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int top = metrics.topPadding + metrics.headerHeight + 50;
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Computer difficulty");
  const char* opts[] = {tr(STR_SUDOKU_EASY), tr(STR_SUDOKU_NORMAL), tr(STR_SUDOKU_HARD)};
  for (int i = 0; i < 3; ++i) {
    const bool sel = (i == static_cast<int>(difficulty));
    const int y = top + i * rowH;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, opts[i], !sel);
  }
}

void ConnectFourActivity::drawBoard() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = metrics.topPadding + metrics.headerHeight + 20;
  const int availW = renderer.getScreenWidth() - 32;
  const int availH = renderer.getScreenHeight() - top - metrics.buttonHintsHeight - 24;
  const int lipH = 22;
  const int cell = std::max(20, std::min(availW / COLS, (availH - lipH) / ROWS));
  const int boardW = cell * COLS;
  const int boardH = cell * ROWS;
  const int left = (renderer.getScreenWidth() - boardW) / 2;
  const int lipTop = top;
  const int gridTop = lipTop + lipH;
  const int frame = 4;

  // Board frame/slab (CrossPlay-inspired: framed dithered slab with holes/discs)
  renderer.drawRect(left - frame, lipTop - frame, boardW + frame * 2, boardH + lipH + frame * 2, 2, true);
  renderer.fillRectDither(left, lipTop, boardW, boardH + lipH, Color::LightGray);
  renderer.drawLine(left, gridTop, left + boardW, gridTop, true);

  // Lip row: show waiting disc above selected column when it is the player's move
  const bool canDropNow = (state == State::Playing) && !aiThinking &&
                          (mode == Mode::PvP || (mode == Mode::VsAI && current == Cell::P1));
  if (canDropNow) {
    const Cell preview = (mode == Mode::PvP) ? current : Cell::P1;
    const int cx = left + cursorCol * cell + cell / 2;
    const int cy = lipTop + lipH / 2;
    const int r = std::max(5, cell / 2 - 6);
    if (preview == Cell::P2) {
      renderer.fillRoundedRect(cx - r, cy - r, r * 2, r * 2, r, Color::Black);
    } else {
      const int ir = std::max(2, r - 4);
      renderer.fillRoundedRect(cx - r, cy - r, r * 2, r * 2, r, Color::LightGray);
      renderer.drawRoundedRect(cx - r, cy - r, r * 2, r * 2, 2, r, true);
      renderer.drawRoundedRect(cx - ir, cy - ir, ir * 2, ir * 2, 1, ir, true);
    }
  } else {
    // subtle cursor marker when not actively dropping
    const int cx = left + cursorCol * cell + cell / 2;
    renderer.fillRoundedRect(cx - 4, lipTop + lipH / 2 - 4, 8, 8, 4, Color::DarkGray);
  }

  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS; ++c) {
      const int x = left + c * cell;
      const int y = gridTop + r * cell;
      const int cx = x + cell / 2;
      const int cy = y + cell / 2;
      const int holeR = std::max(6, cell / 2 - 4);
      const int discR = std::max(5, holeR - 1);

      // Hole
      renderer.fillRoundedRect(cx - holeR, cy - holeR, holeR * 2, holeR * 2, holeR, Color::White);
      renderer.drawRoundedRect(cx - holeR, cy - holeR, holeR * 2, holeR * 2, 1, holeR, true);

      const Cell v = board[r * COLS + c];
      if (v == Cell::P1) {
        // Player 1: medium-light disc with double rim (distinct from board)
        const int innerR = std::max(3, discR - 5);
        renderer.fillRoundedRect(cx - discR, cy - discR, discR * 2, discR * 2, discR, Color::LightGray);
        renderer.drawRoundedRect(cx - discR, cy - discR, discR * 2, discR * 2, 3, discR, true);
        renderer.drawRoundedRect(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 2, innerR, true);
      } else if (v == Cell::P2) {
        // Player 2 / dark disc: solid black
        renderer.fillRoundedRect(cx - discR, cy - discR, discR * 2, discR * 2, discR, Color::Black);
      }
    }
  }

  // Column guides (subtle)
  for (int c = 1; c < COLS; ++c) {
    const int x = left + c * cell;
    renderer.drawLine(x, gridTop, x, gridTop + boardH, true);
  }

  const int statusY = gridTop + boardH + 10;
  if (state == State::P1Win) {
    renderer.drawCenteredText(UI_12_FONT_ID, statusY, mode == Mode::PvP ? "Player 1 wins!" : tr(STR_CONNECT_FOUR_WIN));
  } else if (state == State::P2Win) {
    renderer.drawCenteredText(UI_12_FONT_ID, statusY, mode == Mode::PvP ? "Player 2 wins!" : tr(STR_CONNECT_FOUR_LOSE));
  } else if (state == State::Draw) {
    renderer.drawCenteredText(UI_12_FONT_ID, statusY, tr(STR_CONNECT_FOUR_DRAW));
  } else if (mode == Mode::PvP) {
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, current == Cell::P1 ? "Turn: Player 1" : "Turn: Player 2");
  } else if (aiThinking) {
    renderer.drawCenteredText(UI_12_FONT_ID, statusY, "Thinking...");
  } else {
    const char* diff = difficulty == Difficulty::Easy ? tr(STR_SUDOKU_EASY) : (difficulty == Difficulty::Normal ? tr(STR_SUDOKU_NORMAL) : tr(STR_SUDOKU_HARD));
    char buf[48];
    snprintf(buf, sizeof(buf), "You vs Computer (%s)", diff);
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, buf);
  }
}

void ConnectFourActivity::drawStartMenu() const {
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

void ConnectFourActivity::render(RenderLock&&) {
  needsRedraw = false;
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_CONNECT_FOUR));
  if (screen == Screen::StartMenu) drawStartMenu();
  else if (screen == Screen::ModeSelect) drawModeSelect();
  else if (screen == Screen::DifficultySelect) drawDifficultySelect();
  else drawBoard();
  const char* hint = tr(STR_SELECT);
  if (screen == Screen::Playing && state != State::Playing) hint = tr(STR_NEW_GAME);
  else if (screen == Screen::Playing && aiThinking) hint = "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), hint, aiThinking ? "" : tr(STR_DIR_LEFT), aiThinking ? "" : tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
