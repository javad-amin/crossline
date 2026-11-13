#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"

class SnakeActivity final : public Activity {
 public:
  SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Point {
    int8_t x = 0;
    int8_t y = 0;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
  };

  enum class State : uint8_t { Ready, Playing, Paused, GameOver };
  enum class Screen : uint8_t { StartMenu, Playing };

  void resetGame();
  void continueGame();
  void saveProgress() const;
  void placeFood();
  void step();
  void setDirection(int8_t dx, int8_t dy);
  bool isOpposite(int8_t dx, int8_t dy) const;
  bool occupies(const Point& p, bool includeTail) const;
  void drawStartMenu() const;
  void drawBoard() const;
  int startMenuTop() const;

  static constexpr int GRID_W = 16;
  static constexpr int GRID_H = 16;
  static constexpr int MAX_LEN = GRID_W * GRID_H;

  Screen screen = Screen::Playing;
  State state = State::Ready;
  std::array<Point, MAX_LEN> snake{};
  int snakeLen = 0;
  int headIndex = 0;
  Point food{8, 8};
  int8_t dirX = 1;
  int8_t dirY = 0;
  int8_t nextDirX = 1;
  int8_t nextDirY = 0;
  int score = 0;
  unsigned long lastTickMs = 0;
  unsigned long tickIntervalMs = 650;
  uint32_t rngState = 1;
  bool hasSavedGame = false;
  int startSelection = 0;
};
