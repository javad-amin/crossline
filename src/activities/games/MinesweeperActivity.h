#pragma once

#include "Minesweeper.h"
#include "activities/Activity.h"

class MinesweeperActivity final : public Activity {
 public:
  MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Difficulty, Playing, Result };

  void newGame();
  void drawDifficulty();
  void drawBoard();
  void drawResult();
  void drawFooter();
  int boardLeft() const;
  int boardTop() const;
  int cellSize() const;

  Screen screen = Screen::Difficulty;
  minesweeper::Difficulty difficulty = minesweeper::Difficulty::Easy;
  minesweeper::Board board{};
  int cursorX = 0;
  int cursorY = 0;
  uint32_t rngState = 1;
};
