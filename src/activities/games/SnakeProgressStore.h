#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

class SnakeProgressStore : public PersistableStore<SnakeProgressStore> {
  SnakeProgressStore() = default;

  friend class PersistableStore<SnakeProgressStore>;

 public:
  static constexpr int GRID_W = 16;
  static constexpr int GRID_H = 16;
  static constexpr int MAX_LEN = GRID_W * GRID_H;

  struct Point {
    int8_t x = 0;
    int8_t y = 0;
  };

  enum class State : uint8_t { Ready = 0, Playing = 1, Paused = 2, GameOver = 3 };

  static const char* getFilePath() { return "/.crosspoint/snake.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool saveGame(State state, const std::array<Point, MAX_LEN>& snake, int snakeLen, int headIndex, Point food,
                int8_t dirX, int8_t dirY, int8_t nextDirX, int8_t nextDirY, int score, unsigned long tickIntervalMs,
                uint32_t rngState);
  void clear();

  bool hasSavedGame() const { return savedGame; }
  State state() const { return savedState; }
  const std::array<Point, MAX_LEN>& snake() const { return savedSnake; }
  int snakeLen() const { return savedSnakeLen; }
  int headIndex() const { return savedHeadIndex; }
  Point food() const { return savedFood; }
  int8_t dirX() const { return savedDirX; }
  int8_t dirY() const { return savedDirY; }
  int8_t nextDirX() const { return savedNextDirX; }
  int8_t nextDirY() const { return savedNextDirY; }
  int score() const { return savedScore; }
  unsigned long tickIntervalMs() const { return savedTickIntervalMs; }
  uint32_t rngState() const { return savedRngState; }

 private:
  bool savedGame = false;
  State savedState = State::Ready;
  std::array<Point, MAX_LEN> savedSnake{};
  int savedSnakeLen = 0;
  int savedHeadIndex = 0;
  Point savedFood{8, 8};
  int8_t savedDirX = 1;
  int8_t savedDirY = 0;
  int8_t savedNextDirX = 1;
  int8_t savedNextDirY = 0;
  int savedScore = 0;
  unsigned long savedTickIntervalMs = 650;
  uint32_t savedRngState = 1;
};
