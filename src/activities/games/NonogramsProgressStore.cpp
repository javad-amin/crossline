#include "NonogramsProgressStore.h"

#include <Logging.h>

#include <algorithm>

namespace {
constexpr int PROGRESS_VERSION = 1;
}

void NonogramsProgressStore::toJson(JsonDocument& doc) const {
  doc["version"] = PROGRESS_VERSION;
  doc["active"] = savedGame;
  if (!savedGame) return;

  doc["n"] = savedN;
  doc["difficulty"] = static_cast<uint8_t>(savedDifficulty);
  doc["cursorX"] = savedCursorX;
  doc["cursorY"] = savedCursorY;
  doc["win"] = savedWin;

  JsonArray sol = doc["solution"].to<JsonArray>();
  JsonArray grid = doc["grid"].to<JsonArray>();
  const int total = savedN * savedN;
  for (int i = 0; i < total; ++i) {
    sol.add(savedSolution[i]);
    grid.add(static_cast<uint8_t>(savedGrid[i]));
  }
}

bool NonogramsProgressStore::fromJson(const JsonVariantConst doc) {
  savedGame = false;
  if ((doc["version"] | 0) != PROGRESS_VERSION || !doc["active"].is<bool>()) {
    LOG_ERR("NONO", "Unsupported or invalid saved game format");
    return false;
  }
  if (!doc["active"].as<bool>()) return true;

  const int n = doc["n"] | 0;
  const int difficulty = doc["difficulty"] | -1;
  const int cursorX = doc["cursorX"] | 0;
  const int cursorY = doc["cursorY"] | 0;
  if (n != 5 && n != 10 && n != 15) {
    LOG_ERR("NONO", "Invalid saved size");
    return false;
  }
  if (difficulty < 0 || difficulty > 2 || !doc["win"].is<bool>()) {
    LOG_ERR("NONO", "Invalid saved metadata");
    return false;
  }
  const JsonArrayConst solArr = doc["solution"].as<JsonArrayConst>();
  const JsonArrayConst gridArr = doc["grid"].as<JsonArrayConst>();
  const int total = n * n;
  if (solArr.size() != static_cast<size_t>(total) || gridArr.size() != static_cast<size_t>(total)) {
    LOG_ERR("NONO", "Invalid saved arrays");
    return false;
  }

  std::array<uint8_t, MAX_N * MAX_N> sol{};
  std::array<Mark, MAX_N * MAX_N> grid{};
  int idx = 0;
  for (const JsonVariantConst v : solArr) {
    if (!v.is<int>()) return false;
    const int b = v.as<int>();
    if (b != 0 && b != 1) return false;
    sol[idx++] = static_cast<uint8_t>(b);
  }
  idx = 0;
  for (const JsonVariantConst v : gridArr) {
    if (!v.is<int>()) return false;
    const int m = v.as<int>();
    if (m < 0 || m > 2) return false;
    grid[idx++] = static_cast<Mark>(m);
  }

  // basic consistency: filled marks can't contradict solution? allow player marks freely except win flag is recomputed-ish
  savedN = n;
  savedDifficulty = static_cast<Difficulty>(difficulty);
  savedSolution = sol;
  savedGrid = grid;
  savedCursorX = std::clamp(cursorX, 0, n - 1);
  savedCursorY = std::clamp(cursorY, 0, n - 1);
  savedWin = doc["win"].as<bool>();
  savedGame = true;
  return true;
}

bool NonogramsProgressStore::saveGame(int n, Difficulty difficulty, const std::array<uint8_t, MAX_N * MAX_N>& solution,
                                      const std::array<Mark, MAX_N * MAX_N>& grid, int cursorX, int cursorY, bool win) {
  if (n != 5 && n != 10 && n != 15) return false;
  savedN = n;
  savedDifficulty = difficulty;
  savedSolution = solution;
  savedGrid = grid;
  savedCursorX = std::clamp(cursorX, 0, n - 1);
  savedCursorY = std::clamp(cursorY, 0, n - 1);
  savedWin = win;
  savedGame = true;
  if (!saveToFile()) {
    LOG_ERR("NONO", "Failed to save game progress");
    return false;
  }
  return true;
}

void NonogramsProgressStore::clear() {
  savedGame = false;
  saveToFile();
}
