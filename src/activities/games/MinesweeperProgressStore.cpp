#include "MinesweeperProgressStore.h"

#include <Logging.h>

#include <algorithm>

namespace {
constexpr int PROGRESS_VERSION = 1;

template <size_t N>
bool readU8Array(const JsonVariantConst value, std::array<uint8_t, N>& out, int count, int maxValue) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.size() != static_cast<size_t>(count)) return false;
  int index = 0;
  for (const JsonVariantConst item : array) {
    if (!item.is<int>()) return false;
    const int v = item.as<int>();
    if (v < 0 || v > maxValue) return false;
    out[index++] = static_cast<uint8_t>(v);
  }
  return true;
}
}  // namespace

void MinesweeperProgressStore::toJson(JsonDocument& doc) const {
  doc["version"] = PROGRESS_VERSION;
  doc["active"] = savedGame;
  if (!savedGame) return;

  doc["difficulty"] = static_cast<uint8_t>(savedDifficulty);
  doc["w"] = savedBoard.cfg.w;
  doc["h"] = savedBoard.cfg.h;
  doc["mines"] = savedBoard.cfg.mines;
  doc["generated"] = savedBoard.generated;
  doc["exploded"] = savedBoard.exploded;
  doc["won"] = savedBoard.won;
  doc["revealedCount"] = savedBoard.revealedCount;
  doc["cursorX"] = savedCursorX;
  doc["cursorY"] = savedCursorY;

  const int count = savedBoard.cfg.w * savedBoard.cfg.h;
  JsonArray mine = doc["mine"].to<JsonArray>();
  JsonArray revealed = doc["revealed"].to<JsonArray>();
  JsonArray flagged = doc["flagged"].to<JsonArray>();
  JsonArray adjacent = doc["adjacent"].to<JsonArray>();
  for (int i = 0; i < count; ++i) {
    mine.add(savedBoard.mine[i]);
    revealed.add(savedBoard.revealed[i]);
    flagged.add(savedBoard.flagged[i]);
    adjacent.add(savedBoard.adjacent[i]);
  }
}

bool MinesweeperProgressStore::fromJson(const JsonVariantConst doc) {
  savedGame = false;
  if ((doc["version"] | 0) != PROGRESS_VERSION || !doc["active"].is<bool>()) {
    LOG_ERR("MINES", "Unsupported or invalid saved game format");
    return false;
  }
  if (!doc["active"].as<bool>()) return true;

  const int difficulty = doc["difficulty"] | -1;
  const int w = doc["w"] | 0;
  const int h = doc["h"] | 0;
  const int mines = doc["mines"] | -1;
  const int revealedCount = doc["revealedCount"] | -1;
  const int cursorX = doc["cursorX"] | 0;
  const int cursorY = doc["cursorY"] | 0;
  if (difficulty < 0 || difficulty > 3 || w < 1 || w > minesweeper::MAX_W || h < 1 || h > minesweeper::MAX_H ||
      mines < 0 || mines >= w * h || revealedCount < 0 || revealedCount > w * h || !doc["generated"].is<bool>() ||
      !doc["exploded"].is<bool>() || !doc["won"].is<bool>()) {
    LOG_ERR("MINES", "Invalid saved game metadata");
    return false;
  }
  const minesweeper::Config cfg{w, h, mines};
  const minesweeper::Config expected = minesweeper::configFor(static_cast<minesweeper::Difficulty>(difficulty));
  if (cfg.w != expected.w || cfg.h != expected.h || cfg.mines != expected.mines) {
    LOG_ERR("MINES", "Config does not match difficulty");
    return false;
  }

  minesweeper::Board b{};
  b.cfg = cfg;
  const int count = w * h;
  if (!readU8Array(doc["mine"], b.mine, count, 1) || !readU8Array(doc["revealed"], b.revealed, count, 1) ||
      !readU8Array(doc["flagged"], b.flagged, count, 1) || !readU8Array(doc["adjacent"], b.adjacent, count, 9)) {
    LOG_ERR("MINES", "Invalid saved arrays");
    return false;
  }
  b.generated = doc["generated"].as<bool>();
  b.exploded = doc["exploded"].as<bool>();
  b.won = doc["won"].as<bool>();
  b.revealedCount = revealedCount;

  int mineCount = 0;
  int actualRevealed = 0;
  for (int i = 0; i < count; ++i) {
    if (b.mine[i]) ++mineCount;
    if (b.revealed[i]) ++actualRevealed;
    if (b.revealed[i] && b.flagged[i]) return false;
    if (b.mine[i] && b.adjacent[i] != 9) return false;
  }
  if (mineCount != mines) return false;
  // revealedCount tracks safe revealed cells in gameplay logic; on explode we reveal all mines too, so allow >=
  if (!b.exploded && actualRevealed != revealedCount) return false;

  savedBoard = b;
  savedCursorX = std::clamp(cursorX, 0, w - 1);
  savedCursorY = std::clamp(cursorY, 0, h - 1);
  savedDifficulty = static_cast<minesweeper::Difficulty>(difficulty);
  savedGame = true;
  return true;
}

bool MinesweeperProgressStore::saveGame(const minesweeper::Board& board, int cursorX, int cursorY,
                                        minesweeper::Difficulty difficulty) {
  savedBoard = board;
  savedCursorX = std::clamp(cursorX, 0, board.cfg.w - 1);
  savedCursorY = std::clamp(cursorY, 0, board.cfg.h - 1);
  savedDifficulty = difficulty;
  savedGame = true;
  if (!saveToFile()) {
    LOG_ERR("MINES", "Failed to save game progress");
    return false;
  }
  return true;
}

void MinesweeperProgressStore::clear() {
  savedGame = false;
  saveToFile();
}
