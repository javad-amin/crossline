#pragma once

#include "activities/Activity.h"

class GamesActivity final : public Activity {
 public:
  GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Tab : uint8_t { Games = 0, Apps, Count };
  enum class GameChoice : uint8_t { Chess = 0, ConnectFour, Minesweeper, Nonograms, Snake, Sudoku, Count };
  enum class AppChoice : uint8_t { Calculator = 0, Stopwatch, Count };

  void openSelected();
  int tabBarTop() const;
  int tabBarHeight() const;
  int listTop() const;
  int rowHeight() const;
  int currentCount() const;
  void drawTabs() const;

  Tab activeTab = Tab::Games;
  int selectedIndex = 0;
  int appSelectedIndex = 0;
};
