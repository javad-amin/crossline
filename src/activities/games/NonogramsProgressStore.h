#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

class NonogramsProgressStore : public PersistableStore<NonogramsProgressStore> {
  NonogramsProgressStore() = default;

  friend class PersistableStore<NonogramsProgressStore>;

 public:
  static constexpr int MAX_N = 15;

  enum class Mark : uint8_t { Empty, Filled, Cross };
  enum class Difficulty : uint8_t { Easy = 0, Normal = 1, Hard = 2 };

  static const char* getFilePath() { return "/.crosspoint/nonograms.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool saveGame(int n, Difficulty difficulty, const std::array<uint8_t, MAX_N * MAX_N>& solution,
                const std::array<Mark, MAX_N * MAX_N>& grid, int cursorX, int cursorY, bool win);
  void clear();

  bool hasSavedGame() const { return savedGame; }
  int n() const { return savedN; }
  Difficulty difficulty() const { return savedDifficulty; }
  const std::array<uint8_t, MAX_N * MAX_N>& solution() const { return savedSolution; }
  const std::array<Mark, MAX_N * MAX_N>& grid() const { return savedGrid; }
  int cursorX() const { return savedCursorX; }
  int cursorY() const { return savedCursorY; }
  bool win() const { return savedWin; }

 private:
  bool savedGame = false;
  int savedN = 10;
  Difficulty savedDifficulty = Difficulty::Normal;
  std::array<uint8_t, MAX_N * MAX_N> savedSolution{};
  std::array<Mark, MAX_N * MAX_N> savedGrid{};
  int savedCursorX = 0;
  int savedCursorY = 0;
  bool savedWin = false;
};
