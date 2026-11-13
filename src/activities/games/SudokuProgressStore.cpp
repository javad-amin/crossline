#include "SudokuProgressStore.h"

#include <Logging.h>

namespace {
constexpr int PROGRESS_VERSION = 1;

bool readBoard(const JsonVariantConst value, sudoku::Board& board, const bool allowZero) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.size() != sudoku::CELL_COUNT) return false;
  int index = 0;
  for (const JsonVariantConst item : array) {
    if (!item.is<int>()) return false;
    const int digit = item.as<int>();
    if (digit < (allowZero ? 0 : 1) || digit > sudoku::SIDE) return false;
    board[index++] = static_cast<uint8_t>(digit);
  }
  return true;
}

bool readNotes(const JsonVariantConst value, sudoku::Notes& notes) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.size() != sudoku::CELL_COUNT) return false;
  int index = 0;
  for (const JsonVariantConst item : array) {
    if (!item.is<int>()) return false;
    const int mask = item.as<int>();
    if (mask < 0 || mask > sudoku::ALL_DIGITS) return false;
    notes[index++] = static_cast<uint16_t>(mask);
  }
  return true;
}
}  // namespace

void SudokuProgressStore::toJson(JsonDocument& doc) const {
  doc["version"] = PROGRESS_VERSION;
  doc["active"] = savedGame;
  if (!savedGame) return;

  doc["difficulty"] = static_cast<uint8_t>(savedDifficulty);
  doc["pencilMode"] = savedPencilMode;
  JsonArray givens = doc["givens"].to<JsonArray>();
  JsonArray solution = doc["solution"].to<JsonArray>();
  JsonArray entries = doc["entries"].to<JsonArray>();
  JsonArray notes = doc["notes"].to<JsonArray>();
  for (int cell = 0; cell < sudoku::CELL_COUNT; ++cell) {
    givens.add(savedPuzzle.givens[cell]);
    solution.add(savedPuzzle.solution[cell]);
    entries.add(savedEntries[cell]);
    notes.add(savedNotes[cell]);
  }
}

bool SudokuProgressStore::fromJson(const JsonVariantConst doc) {
  savedGame = false;
  if ((doc["version"] | 0) != PROGRESS_VERSION || !doc["active"].is<bool>()) {
    LOG_ERR("SUDOKU", "Unsupported or invalid saved game format");
    return false;
  }
  if (!doc["active"].as<bool>()) return true;

  const int difficulty = doc["difficulty"] | -1;
  if (difficulty < static_cast<int>(sudoku::Difficulty::Easy) ||
      difficulty > static_cast<int>(sudoku::Difficulty::Dev) || !doc["pencilMode"].is<bool>() ||
      !readBoard(doc["givens"], savedPuzzle.givens, true) ||
      !readBoard(doc["solution"], savedPuzzle.solution, false) ||
      !readBoard(doc["entries"], savedEntries, true) || !readNotes(doc["notes"], savedNotes) ||
      !sudoku::isValidSolution(savedPuzzle.solution)) {
    LOG_ERR("SUDOKU", "Invalid saved game data");
    return false;
  }
  for (int cell = 0; cell < sudoku::CELL_COUNT; ++cell) {
    if ((savedPuzzle.givens[cell] != 0 && savedPuzzle.givens[cell] != savedPuzzle.solution[cell]) ||
        (savedPuzzle.givens[cell] != 0 && savedEntries[cell] != savedPuzzle.givens[cell])) {
      LOG_ERR("SUDOKU", "Inconsistent saved game data");
      return false;
    }
  }

  savedDifficulty = static_cast<sudoku::Difficulty>(difficulty);
  savedPencilMode = doc["pencilMode"].as<bool>();
  savedGame = true;
  return true;
}

bool SudokuProgressStore::saveGame(const sudoku::Puzzle& puzzle, const sudoku::Board& entries,
                                   const sudoku::Notes& notes, const sudoku::Difficulty difficulty,
                                   const bool pencilMode) {
  savedPuzzle = puzzle;
  savedEntries = entries;
  savedNotes = notes;
  savedDifficulty = difficulty;
  savedPencilMode = pencilMode;
  savedGame = true;
  if (!saveToFile()) {
    LOG_ERR("SUDOKU", "Failed to save game progress");
    return false;
  }
  return true;
}
