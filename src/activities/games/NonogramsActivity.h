#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"

class NonogramsActivity final : public Activity {
 public:
  NonogramsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int MAX_N = 15;
  enum class Mark : uint8_t { Empty, Filled, Cross };
  enum class Difficulty : uint8_t { Easy = 0, Normal = 1, Hard = 2 };
  enum class Screen : uint8_t { StartMenu, Difficulty, Playing };

  void reset();
  void generatePuzzle();
  bool solved() const;
  int gridSize() const;
  void drawStartMenu() const;
  void drawDifficulty() const;
  void drawBoard() const;
  void continueGame();
  void saveProgress() const;
  int startMenuTop() const;

  Difficulty difficulty = Difficulty::Normal;
  Screen screen = Screen::Difficulty;
  int n = 10;
  std::array<uint8_t, MAX_N * MAX_N> solution{};  // 0/1
  std::array<Mark, MAX_N * MAX_N> grid{};
  int cursorX = 0, cursorY = 0;
  bool win = false;
  uint32_t rngState = 1;
  bool needsRedraw = true;
  bool hasSavedGame = false;
  int startSelection = 0;
};
