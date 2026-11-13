#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

#include "Minesweeper.h"

class MinesweeperProgressStore : public PersistableStore<MinesweeperProgressStore> {
  MinesweeperProgressStore() = default;

  friend class PersistableStore<MinesweeperProgressStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/minesweeper.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool saveGame(const minesweeper::Board& board, int cursorX, int cursorY, minesweeper::Difficulty difficulty);
  void clear();

  bool hasSavedGame() const { return savedGame; }
  const minesweeper::Board& board() const { return savedBoard; }
  int cursorX() const { return savedCursorX; }
  int cursorY() const { return savedCursorY; }
  minesweeper::Difficulty difficulty() const { return savedDifficulty; }

 private:
  bool savedGame = false;
  minesweeper::Board savedBoard{};
  int savedCursorX = 0;
  int savedCursorY = 0;
  minesweeper::Difficulty savedDifficulty = minesweeper::Difficulty::Easy;
};
