#include "ChessActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ChessPieces.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
int fileOf(int sq) { return sq & 7; }
int rankOf(int sq) { return sq >> 3; }
int sqAt(int f, int r) { return r * 8 + f; }

void blit1bpp(GfxRenderer& renderer, const uint8_t* bits, int size, int x, int y) {
  if (!bits) return;
  const int rowBytes = (size + 7) / 8;
  for (int ry = 0; ry < size; ++ry) {
    for (int rx = 0; rx < size; ++rx) {
      const uint8_t b = bits[ry * rowBytes + (rx >> 3)];
      if ((b & (0x80 >> (rx & 7))) != 0) renderer.drawPixel(x + rx, y + ry, true);
    }
  }
}

const uint8_t* solidPiece(uint8_t type, bool small) {
  switch (type) {
    case chess::Pawn: return small ? chessart::kPawnSolidSmall : chessart::kPawnSolid;
    case chess::Knight: return small ? chessart::kKnightSolidSmall : chessart::kKnightSolid;
    case chess::Bishop: return small ? chessart::kBishopSolidSmall : chessart::kBishopSolid;
    case chess::Rook: return small ? chessart::kRookSolidSmall : chessart::kRookSolid;
    case chess::Queen: return small ? chessart::kQueenSolidSmall : chessart::kQueenSolid;
    case chess::King: return small ? chessart::kKingSolidSmall : chessart::kKingSolid;
    default: return nullptr;
  }
}
const uint8_t* outlinePiece(uint8_t type, bool small) {
  switch (type) {
    case chess::Pawn: return small ? chessart::kPawnLightSmall : chessart::kPawnLight;
    case chess::Knight: return small ? chessart::kKnightLightSmall : chessart::kKnightLight;
    case chess::Bishop: return small ? chessart::kBishopLightSmall : chessart::kBishopLight;
    case chess::Rook: return small ? chessart::kRookLightSmall : chessart::kRookLight;
    case chess::Queen: return small ? chessart::kQueenLightSmall : chessart::kQueenLight;
    case chess::King: return small ? chessart::kKingLightSmall : chessart::kKingLight;
    default: return nullptr;
  }
}
}  // namespace

ChessActivity::ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Chess", renderer, mappedInput) {}

void ChessActivity::onEnter() {
  Activity::onEnter();
  loadSettings();
  hasSavedGame = loadGame();
  if (!hasSavedGame) {
    resetGame();
    hasSavedGame = false;
  }
  screen = hasSavedGame ? Screen::StartMenu : Screen::ModeSelect;
  menuSelection = 0;
  needsRedraw = true;
  requestUpdate();
}

void ChessActivity::onExit() {
  if (state == State::Playing) saveGame();
  saveSettings();
  Activity::onExit();
}

void ChessActivity::loadSettings() {
  char buf[64] = {};
  if (Storage.readFileToBuffer(kSettingsPath, buf, sizeof(buf)) == 0) return;
  int v = 0, m = 0, lvl = chess::kDefaultLevel, hw = 1, hints = 1;
  if (sscanf(buf, "%d %d %d %d %d", &v, &m, &lvl, &hw, &hints) >= 5 && v == kSaveVersion) {
    if (m >= 0 && m <= 2) mode = static_cast<Mode>(m);
    level = static_cast<uint8_t>(chess::levelFromStored(lvl));
    humanPlaysWhite = hw != 0;
    showHints = hints != 0;
  }
}

void ChessActivity::saveSettings() const {
  char buf[64];
  snprintf(buf, sizeof(buf), "%d %d %d %d %d\n", kSaveVersion, static_cast<int>(mode),
           chess::kStoredLevelBase + level, humanPlaysWhite ? 1 : 0, showHints ? 1 : 0);
  Storage.writeFile(kSettingsPath, String(buf));
}

bool ChessActivity::loadGame() {
  if (!Storage.exists(kSavePath)) return false;
  std::unique_ptr<char[]> buffer(new (std::nothrow) char[2048]);
  if (!buffer) return false;
  const size_t read = Storage.readFileToBuffer(kSavePath, buffer.get(), 2047);
  if (read == 0) return false;
  buffer[read] = '\0';
  char* cur = buffer.get();
  auto nextLine = [&cur]() -> char* {
    if (!cur || !*cur) return nullptr;
    char* sline = cur;
    while (*cur && *cur != '\n') ++cur;
    if (*cur == '\n') *cur++ = '\0';
    return sline;
  };
  const char* ver = nextLine();
  if (!ver || atoi(ver) != kSaveVersion) return false;
  const char* modeLine = nextLine();
  const char* fen = nextLine();
  const char* base = nextLine();
  if (!modeLine || !fen || !base) return false;
  chess::Position restored;
  if (!chess::parseFen(fen, restored)) return false;
  const int m = atoi(modeLine);
  if (m >= 0 && m <= 2) mode = static_cast<Mode>(m);
  position = restored;
  historyBase = atoi(base);
  historyCount = 0;
  for (char* line = nextLine(); line && historyCount < kMaxHistory; line = nextLine()) {
    if (*line == '\0') continue;
    snprintf(history[historyCount], sizeof(history[0]), "%s", line);
    historyCount++;
  }
  selectedSquare = -1;
  cursorSquare = humanPlaysWhite ? 12 : 52;
  engineThinking = false;
  undoableFrom = historyCount;
  resetRepetition();
  refreshLegalMoves();
  return state == State::Playing || historyCount > 0;
}

void ChessActivity::saveGame() const {
  if (state != State::Playing && historyCount == 0) return;
  HalFile file;
  if (!Storage.openFileForWrite("CHESS", kSavePath, file)) return;
  char line[128];
  snprintf(line, sizeof(line), "%d\n", kSaveVersion);
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  snprintf(line, sizeof(line), "%d\n", static_cast<int>(mode));
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  char fen[96];
  chess::positionToFen(position, fen);
  snprintf(line, sizeof(line), "%s\n%d\n", fen, historyBase);
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  for (int i = 0; i < historyCount; ++i) {
    snprintf(line, sizeof(line), "%s\n", history[i]);
    file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  }
  file.close();
}

void ChessActivity::resetGame() {
  Storage.remove(kSavePath);
  chess::setStartPosition(position);
  selectedSquare = -1;
  cursorSquare = humanPlaysWhite ? 12 : 52;
  historyCount = 0;
  historyBase = 0;
  undoableFrom = 0;
  engineThinking = false;
  state = State::Playing;
  resetRepetition();
  refreshLegalMoves();
  hasSavedGame = true;
  if (engineToMove()) engineThinking = true;
}

void ChessActivity::resetRepetition() {
  repCount = 0;
  pushRepetitionKey();
}

void ChessActivity::pushRepetitionKey() {
  if (repCount >= static_cast<int>(repKeys.size())) {
    for (int i = 1; i < repCount; ++i) repKeys[i - 1] = repKeys[i];
    repCount--;
  }
  repKeys[repCount++] = chess::positionKey(position);
}

void ChessActivity::refreshLegalMoves() {
  chess::generateLegalMoves(position, legalMoves);
  if (legalMoves.count == 0) {
    if (chess::isInCheck(position)) state = State::Checkmate;
    else state = State::Stalemate;
  } else {
    // basic draw claims not tracked deeply except terminal no-move; keep simple
    state = State::Playing;
  }
}

bool ChessActivity::engineToMove() const {
  return vsComputer() && state == State::Playing && (position.whiteToMove != humanPlaysWhite);
}

bool ChessActivity::whiteAtBottom() const {
  if (mode == Mode::FaceToFace) return true;
  if (vsComputer()) return humanPlaysWhite;
  // Pass and play: side to move at bottom
  return position.whiteToMove;
}

void ChessActivity::recordMove(const chess::Move& move) {
  if (historyCount >= kMaxHistory) {
    for (int i = 1; i < kMaxHistory; ++i) {
      memcpy(history[i - 1], history[i], sizeof(history[0]));
      historyMove[i - 1] = historyMove[i];
      historyUndo[i - 1] = historyUndo[i];
    }
    historyCount = kMaxHistory - 1;
    historyBase++;
    if (undoableFrom > 0) undoableFrom--;
  }
  chess::moveToSan(position, move, history[historyCount]);
}

void ChessActivity::applyMove(const chess::Move& move) {
  recordMove(move);
  chess::Undo undo;
  chess::makeMove(position, move, undo);
  historyMove[historyCount] = move;
  historyUndo[historyCount] = undo;
  historyCount++;
  pushRepetitionKey();
  selectedSquare = -1;
  refreshLegalMoves();
  hasSavedGame = true;
  saveGame();
  if (engineToMove()) engineThinking = true;
  needsRedraw = true;
}

bool ChessActivity::tryMove(int from, int to) {
  for (int i = 0; i < legalMoves.count; ++i) {
    if (legalMoves.moves[i].from == from && legalMoves.moves[i].to == to) {
      applyMove(legalMoves.moves[i]);
      return true;
    }
  }
  return false;
}

void ChessActivity::handleSquareActivated(int sq) {
  if (state != State::Playing || engineToMove()) return;
  if (selectedSquare < 0) {
    const uint8_t piece = position.squares[sq];
    if (piece != chess::Empty && chess::isColour(piece, position.whiteToMove)) selectedSquare = sq;
  } else {
    if (sq == selectedSquare) {
      selectedSquare = -1;
    } else if (!tryMove(selectedSquare, sq)) {
      const uint8_t piece = position.squares[sq];
      if (piece != chess::Empty && chess::isColour(piece, position.whiteToMove)) selectedSquare = sq;
      else selectedSquare = -1;
    }
  }
  needsRedraw = true;
}

void ChessActivity::playEngineMove() {
  engineThinking = false;
  if (!engineToMove()) { needsRedraw = true; return; }
  chess::Position copy = position;
  const auto& style = chess::kLevels[level];
  const uint32_t seed = static_cast<uint32_t>(millis()) ^ (static_cast<uint32_t>(historyCount) * 2654435761u);
  chess::SearchResult res = chess::searchStyled(copy, style, searchBuffers, seed ? seed : 1u);
  if (res.hasMove) applyMove(res.best);
  else { refreshLegalMoves(); needsRedraw = true; }
}

bool ChessActivity::canTakeBack() const {
  if (engineThinking) return false;
  if (vsComputer()) return historyCount > undoableFrom;
  return historyCount > undoableFrom;
}

void ChessActivity::takeBack() {
  if (!canTakeBack()) return;
  int plies = 1;
  if (vsComputer()) plies = engineToMove() ? 1 : 2;
  for (int i = 0; i < plies && historyCount > undoableFrom; ++i) {
    historyCount--;
    chess::unmakeMove(position, historyMove[historyCount], historyUndo[historyCount]);
    if (repCount > 1) repCount--;
  }
  selectedSquare = -1;
  refreshLegalMoves();
  saveGame();
  needsRedraw = true;
}

const char* ChessActivity::statusText() const {
  static char buf[48];
  if (state == State::Checkmate) {
    snprintf(buf, sizeof(buf), "Checkmate - %s wins", position.whiteToMove ? "Black" : "White");
    return buf;
  }
  if (state == State::Stalemate) return "Stalemate / Draw";
  if (state == State::Draw) return "Draw";
  if (engineThinking) return "Thinking...";
  if (chess::isInCheck(position)) {
    snprintf(buf, sizeof(buf), "Check - %s to move", position.whiteToMove ? "White" : "Black");
    return buf;
  }
  snprintf(buf, sizeof(buf), "%s to move", position.whiteToMove ? "White" : "Black");
  return buf;
}

int ChessActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

int ChessActivity::modeMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + 50;
}
int ChessActivity::difficultyMenuTop() const { return modeMenuTop(); }
int ChessActivity::sideMenuTop() const { return modeMenuTop(); }

void ChessActivity::loop() {
  if (engineThinking && engineToMove() && screen == Screen::Playing) {
    playEngineMove();
  }

  switch (screen) {
    case Screen::StartMenu: handleStartMenuInput(); break;
    case Screen::ModeSelect: handleModeSelectInput(); break;
    case Screen::DifficultySelect: handleDifficultySelectInput(); break;
    case Screen::SideSelect: handleSideSelectInput(); break;
    case Screen::Playing: handlePlayingInput(); break;
  }

  if (needsRedraw) {
    needsRedraw = false;
    requestUpdate();
  }
}

void ChessActivity::handleStartMenuInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    menuSelection = 1 - menuSelection; needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (menuSelection == 0 && hasSavedGame) { screen = Screen::Playing; }
    else { screen = Screen::ModeSelect; menuSelection = 0; }
    needsRedraw = true; return;
  }
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
    const int top = startMenuTop();
    const int idx = (ty - top) / rowHeight;
    const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
    if (tx >= margin && tx < renderer.getScreenWidth() - margin && idx >= 0 && idx < 2 && ty >= top) {
      menuSelection = idx;
      if (menuSelection == 0 && hasSavedGame) { screen = Screen::Playing; }
      else { screen = Screen::ModeSelect; menuSelection = 0; }
      needsRedraw = true;
    }
  }
}

void ChessActivity::handleModeSelectInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (hasSavedGame) { screen = Screen::StartMenu; menuSelection = 0; }
    else { finish(); return; }
    needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    mode = static_cast<Mode>((static_cast<int>(mode) + 2) % 3); needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    mode = static_cast<Mode>((static_cast<int>(mode) + 1) % 3); needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (mode == Mode::VsComputer) { screen = Screen::DifficultySelect; menuSelection = level; }
    else { screen = Screen::SideSelect; menuSelection = humanPlaysWhite ? 0 : 1; }
    needsRedraw = true; return;
  }
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
    const int top = modeMenuTop();
    const int idx = (ty - top) / rowH;
    if (idx >= 0 && idx < 3) {
      mode = static_cast<Mode>(idx);
      if (mode == Mode::VsComputer) { screen = Screen::DifficultySelect; menuSelection = level; }
      else { screen = Screen::SideSelect; menuSelection = humanPlaysWhite ? 0 : 1; }
      needsRedraw = true;
    }
  }
}

void ChessActivity::handleDifficultySelectInput() {
  const int count = chess::kLevelCount;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { screen = Screen::ModeSelect; needsRedraw = true; return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    level = static_cast<uint8_t>((level + count - 1) % count); needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    level = static_cast<uint8_t>((level + 1) % count); needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { screen = Screen::SideSelect; menuSelection = humanPlaysWhite ? 0 : 1; needsRedraw = true; return; }
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int rowH = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) + 14);
    const int top = difficultyMenuTop();
    const int idx = (ty - top) / rowH;
    if (idx >= 0 && idx < count) { level = static_cast<uint8_t>(idx); screen = Screen::SideSelect; menuSelection = humanPlaysWhite ? 0 : 1; needsRedraw = true; }
  }
}

void ChessActivity::handleSideSelectInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    screen = (mode == Mode::VsComputer) ? Screen::DifficultySelect : Screen::ModeSelect;
    needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    humanPlaysWhite = !humanPlaysWhite; needsRedraw = true; return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    saveSettings();
    resetGame();
    screen = Screen::Playing;
    needsRedraw = true; return;
  }
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
    const int top = sideMenuTop();
    const int idx = (ty - top) / rowH;
    if (idx == 0 || idx == 1) {
      humanPlaysWhite = (idx == 0);
      saveSettings();
      resetGame();
      screen = Screen::Playing;
      needsRedraw = true;
    }
  }
}

void ChessActivity::moveCursorVisual(int dx, int dy) {
  // dx,dy are screen-space moves: dx>0 right, dy>0 down
  const bool bottomWhite = whiteAtBottom();
  int f = fileOf(cursorSquare);
  int r = rankOf(cursorSquare);
  int sf = bottomWhite ? f : 7 - f;
  int sr = bottomWhite ? (7 - r) : r;
  sf = std::clamp(sf + dx, 0, 7);
  sr = std::clamp(sr + dy, 0, 7);
  f = bottomWhite ? sf : 7 - sf;
  r = bottomWhite ? (7 - sr) : sr;
  cursorSquare = sqAt(f, r);
}

void ChessActivity::handlePlayingInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveGame();
    hasSavedGame = true;
    screen = Screen::StartMenu;
    menuSelection = 0;
    needsRedraw = true;
    return;
  }

  if (state != State::Playing) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      screen = Screen::ModeSelect; menuSelection = 0; needsRedraw = true;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) { screen = Screen::ModeSelect; menuSelection = 0; needsRedraw = true; }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    takeBack();
    return;
  }

  bool movedCursor = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { moveCursorVisual(0, -1); movedCursor = true; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { moveCursorVisual(0, 1); movedCursor = true; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) { moveCursorVisual(-1, 0); movedCursor = true; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) { moveCursorVisual(1, 0); movedCursor = true; }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const int sq = squareAtPoint(tx, ty);
    if (sq >= 0) {
      cursorSquare = sq;
      handleSquareActivated(sq);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSquareActivated(cursorSquare);
    return;
  }

  if (movedCursor) needsRedraw = true;
}

int ChessActivity::squareAtPoint(int px, int py) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 28;
  const int bottom = metrics.buttonHintsHeight + 26;
  const int avail = std::min(w - 24, h - top - bottom);
  const int sqSize = std::max(20, avail / 8);
  const int boardPx = sqSize * 8;
  const int ox = (w - boardPx) / 2;
  const int oy = top;
  if (px < ox || py < oy || px >= ox + boardPx || py >= oy + boardPx) return -1;
  const int fx = (px - ox) / sqSize;
  const int ry = (py - oy) / sqSize;
  if (fx < 0 || fx > 7 || ry < 0 || ry > 7) return -1;
  const bool bottomWhite = whiteAtBottom();
  const int file = bottomWhite ? fx : 7 - fx;
  const int rank = bottomWhite ? (7 - ry) : ry;
  return sqAt(file, rank);
}

void ChessActivity::drawPieceAtSquare(int square, int x, int y, int size) const {
  const uint8_t piece = position.squares[square];
  if (piece == chess::Empty) return;
  const uint8_t type = chess::pieceType(piece);
  const bool useLarge = size >= 48;
  const int psize = useLarge ? 48 : 24;
  const uint8_t* solid = solidPiece(type, !useLarge);
  const uint8_t* outline = outlinePiece(type, !useLarge);
  if (!solid || !outline) return;
  const int ox = x + (size - psize) / 2;
  const int oy = y + (size - psize) / 2;
  if (chess::isBlack(piece)) {
    blit1bpp(renderer, solid, psize, ox, oy);
  } else {
    blit1bpp(renderer, outline, psize, ox, oy);
  }
}

void ChessActivity::drawStartMenu() const {
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int top = startMenuTop();
  const char* labels[] = {tr(STR_CONTINUE), tr(STR_NEW_GAME)};
  const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
  for (int i = 0; i < 2; ++i) {
    const int y = top + i * rowHeight;
    const bool selected = (menuSelection == i);
    if (selected) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, labels[i], !selected);
  }
}

void ChessActivity::drawModeSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int top = modeMenuTop();
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Game mode");
  const char* opts[] = {"Vs Computer", "Pass & Play", "Face to Face"};
  for (int i = 0; i < 3; ++i) {
    const bool sel = (i == static_cast<int>(mode));
    const int y = top + i * rowH;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, opts[i], !sel);
  }
}

void ChessActivity::drawDifficultySelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) + 14);
  const int top = difficultyMenuTop();
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Difficulty");
  for (int i = 0; i < chess::kLevelCount; ++i) {
    const bool sel = (i == level);
    const int y = top + i * rowH;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 2);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 2);
    renderer.drawCenteredText(UI_10_FONT_ID, y + (rowH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, chess::kLevels[i].name, !sel);
  }
}

void ChessActivity::drawSideSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int top = sideMenuTop();
  const int margin = std::max(20, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Play as");
  const char* opts[] = {"White", "Black"};
  for (int i = 0; i < 2; ++i) {
    const bool sel = (i == 0 ? humanPlaysWhite : !humanPlaysWhite);
    const int y = top + i * rowH;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, opts[i], !sel);
  }
}

void ChessActivity::drawBoard() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 28;
  const int bottom = metrics.buttonHintsHeight + 26;
  const int avail = std::min(w - 24, h - top - bottom);
  const int sqSize = std::max(20, avail / 8);
  const int boardPx = sqSize * 8;
  const int ox = (w - boardPx) / 2;
  const int oy = top;
  const bool bottomWhite = whiteAtBottom();

  // board squares
  for (int sr = 0; sr < 8; ++sr) {
    for (int sf = 0; sf < 8; ++sf) {
      const int x = ox + sf * sqSize;
      const int y = oy + sr * sqSize;
      const bool dark = ((sf + sr) & 1) != 0;
      if (dark) renderer.fillRectDither(x, y, sqSize, sqSize, LightGray);
      else renderer.fillRect(x, y, sqSize, sqSize, false);
      renderer.drawRect(x, y, sqSize, sqSize, true);
    }
  }

  // selection and hints
  if (selectedSquare >= 0) {
    const int f = fileOf(selectedSquare), r = rankOf(selectedSquare);
    const int sf = bottomWhite ? f : 7 - f;
    const int sr = bottomWhite ? (7 - r) : r;
    renderer.drawRect(ox + sf * sqSize + 1, oy + sr * sqSize + 1, sqSize - 2, sqSize - 2, 2, true);
    if (showHints && state == State::Playing) {
      for (int i = 0; i < legalMoves.count; ++i) {
        const chess::Move& m = legalMoves.moves[i];
        if (m.from != selectedSquare) continue;
        const int tf = bottomWhite ? fileOf(m.to) : 7 - fileOf(m.to);
        const int tr = bottomWhite ? (7 - rankOf(m.to)) : rankOf(m.to);
        const int cx = ox + tf * sqSize + sqSize / 2;
        const int cy = oy + tr * sqSize + sqSize / 2;
        const int dot = std::max(3, sqSize / 8);
        renderer.fillRect(cx - dot / 2, cy - dot / 2, dot, dot, true);
      }
    }
  }

  // cursor
  {
    const int f = fileOf(cursorSquare), r = rankOf(cursorSquare);
    const int sf = bottomWhite ? f : 7 - f;
    const int sr = bottomWhite ? (7 - r) : r;
    renderer.drawRect(ox + sf * sqSize, oy + sr * sqSize, sqSize, sqSize, 2, true);
  }

  // pieces
  for (int sq = 0; sq < 64; ++sq) {
    if (position.squares[sq] == chess::Empty) continue;
    const int f = fileOf(sq), r = rankOf(sq);
    const int sf = bottomWhite ? f : 7 - f;
    const int sr = bottomWhite ? (7 - r) : r;
    drawPieceAtSquare(sq, ox + sf * sqSize, oy + sr * sqSize, sqSize);
  }

  renderer.drawCenteredText(UI_10_FONT_ID, oy + boardPx + 4, statusText());

  // last moves
  int shown = 0;
  char line[64] = {};
  const int listY = oy + boardPx + 24;
  for (int i = std::max(0, historyCount - 6); i < historyCount && shown < 2; i += 2) {
    const int moveNo = (historyBase + i) / 2 + 1;
    if (i + 1 < historyCount) snprintf(line, sizeof(line), "%d. %s %s", moveNo, history[i], history[i + 1]);
    else snprintf(line, sizeof(line), "%d. %s", moveNo, history[i]);
    renderer.drawCenteredText(SMALL_FONT_ID, listY + shown * 16, line);
    shown++;
  }
}

void ChessActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Chess");

  const char* hint = tr(STR_SELECT);
  const char* left = "";
  const char* right = "";
  if (screen == Screen::StartMenu) {
    drawStartMenu();
  } else if (screen == Screen::ModeSelect) {
    drawModeSelect();
  } else if (screen == Screen::DifficultySelect) {
    drawDifficultySelect();
  } else if (screen == Screen::SideSelect) {
    drawSideSelect();
  } else {
    drawBoard();
    if (state != State::Playing) hint = tr(STR_NEW_GAME);
    else if (engineThinking) hint = "";
    else if (canTakeBack()) hint = "Undo/Select";
    left = tr(STR_DIR_LEFT);
    right = tr(STR_DIR_RIGHT);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), hint, left, right);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
