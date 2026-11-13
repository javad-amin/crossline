#include "ConnectFourProgressStore.h"

#include <Logging.h>

#include <algorithm>

namespace {
constexpr int PROGRESS_VERSION = 1;
}

void ConnectFourProgressStore::toJson(JsonDocument& doc) const {
  doc["version"] = PROGRESS_VERSION;
  doc["active"] = savedGame;
  if (!savedGame) return;

  doc["screen"] = static_cast<uint8_t>(savedScreen);
  doc["cursorCol"] = savedCursorCol;
  doc["state"] = static_cast<uint8_t>(savedState);
  doc["mode"] = static_cast<uint8_t>(savedMode);
  doc["difficulty"] = static_cast<uint8_t>(savedDifficulty);
  doc["current"] = static_cast<uint8_t>(savedCurrent);
  doc["rngState"] = savedRngState;

  JsonArray board = doc["board"].to<JsonArray>();
  for (const Cell cell : savedBoard) board.add(static_cast<uint8_t>(cell));
}

bool ConnectFourProgressStore::fromJson(const JsonVariantConst doc) {
  savedGame = false;
  if ((doc["version"] | 0) != PROGRESS_VERSION || !doc["active"].is<bool>()) {
    LOG_ERR("C4", "Unsupported or invalid saved game format");
    return false;
  }
  if (!doc["active"].as<bool>()) return true;

  const int screen = doc["screen"] | -1;
  const int cursorCol = doc["cursorCol"] | -1;
  const int state = doc["state"] | -1;
  const int mode = doc["mode"] | -1;
  const int difficulty = doc["difficulty"] | -1;
  const int current = doc["current"] | -1;
  const JsonArrayConst boardArr = doc["board"].as<JsonArrayConst>();
  if (screen < 0 || screen > 2 || cursorCol < 0 || cursorCol >= COLS || state < 0 || state > 3 || mode < 0 || mode > 1 ||
      difficulty < 0 || difficulty > 2 || current < 1 || current > 2 || boardArr.size() != COLS * ROWS) {
    LOG_ERR("C4", "Invalid saved game data");
    return false;
  }

  std::array<Cell, COLS * ROWS> board{};
  int idx = 0;
  for (const JsonVariantConst v : boardArr) {
    if (!v.is<int>()) return false;
    const int c = v.as<int>();
    if (c < 0 || c > 2) return false;
    board[idx++] = static_cast<Cell>(c);
  }

  savedScreen = static_cast<Screen>(screen);
  savedBoard = board;
  savedCursorCol = cursorCol;
  savedState = static_cast<State>(state);
  savedMode = static_cast<Mode>(mode);
  savedDifficulty = static_cast<Difficulty>(difficulty);
  savedCurrent = static_cast<Cell>(current);
  savedRngState = doc["rngState"] | 1u;
  savedGame = true;
  return true;
}

bool ConnectFourProgressStore::saveGame(Screen screen, const std::array<Cell, COLS * ROWS>& board, int cursorCol,
                                        State state, Mode mode, Difficulty difficulty, Cell current, uint32_t rngState) {
  savedScreen = screen;
  savedBoard = board;
  savedCursorCol = cursorCol;
  savedState = state;
  savedMode = mode;
  savedDifficulty = difficulty;
  savedCurrent = current;
  savedRngState = rngState == 0 ? 1 : rngState;
  savedGame = true;
  if (!saveToFile()) {
    LOG_ERR("C4", "Failed to save game progress");
    return false;
  }
  return true;
}

void ConnectFourProgressStore::clear() {
  savedGame = false;
  saveToFile();
}
