#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include "Sudoku.h"

class SudokuProgressStore : public PersistableStore<SudokuProgressStore> {
  SudokuProgressStore() = default;

  friend class PersistableStore<SudokuProgressStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/sudoku.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool saveGame(const sudoku::Puzzle& puzzle, const sudoku::Board& entries, const sudoku::Notes& notes,
                sudoku::Difficulty difficulty, bool pencilMode);
  bool hasSavedGame() const { return savedGame; }
  const sudoku::Puzzle& puzzle() const { return savedPuzzle; }
  const sudoku::Board& entries() const { return savedEntries; }
  const sudoku::Notes& notes() const { return savedNotes; }
  sudoku::Difficulty difficulty() const { return savedDifficulty; }
  bool pencilMode() const { return savedPencilMode; }

 private:
  bool savedGame = false;
  sudoku::Puzzle savedPuzzle{};
  sudoku::Board savedEntries{};
  sudoku::Notes savedNotes{};
  sudoku::Difficulty savedDifficulty = sudoku::Difficulty::Easy;
  bool savedPencilMode = false;
};
