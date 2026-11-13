#pragma once

#include "activities/Activity.h"

class GamesActivity final : public Activity {
 public:
  GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class GameChoice : uint8_t { Chess = 0, ConnectFour, Minesweeper, Nonograms, Snake, Sudoku, Count };

  void openSelected();
  int listTop() const;
  int rowHeight() const;

  int selectedIndex = 0;
};
