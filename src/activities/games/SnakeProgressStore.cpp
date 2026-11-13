#include "SnakeProgressStore.h"

#include <Logging.h>

#include <algorithm>

namespace {
constexpr int PROGRESS_VERSION = 1;
}

void SnakeProgressStore::toJson(JsonDocument& doc) const {
  doc["version"] = PROGRESS_VERSION;
  doc["active"] = savedGame;
  if (!savedGame) return;

  doc["state"] = static_cast<uint8_t>(savedState);
  doc["snakeLen"] = savedSnakeLen;
  doc["headIndex"] = savedHeadIndex;
  doc["foodX"] = savedFood.x;
  doc["foodY"] = savedFood.y;
  doc["dirX"] = savedDirX;
  doc["dirY"] = savedDirY;
  doc["nextDirX"] = savedNextDirX;
  doc["nextDirY"] = savedNextDirY;
  doc["score"] = savedScore;
  doc["tickIntervalMs"] = savedTickIntervalMs;
  doc["rngState"] = savedRngState;

  JsonArray xs = doc["snakeX"].to<JsonArray>();
  JsonArray ys = doc["snakeY"].to<JsonArray>();
  for (int i = 0; i < MAX_LEN; ++i) {
    xs.add(savedSnake[i].x);
    ys.add(savedSnake[i].y);
  }
}

bool SnakeProgressStore::fromJson(const JsonVariantConst doc) {
  savedGame = false;
  if ((doc["version"] | 0) != PROGRESS_VERSION || !doc["active"].is<bool>()) {
    LOG_ERR("SNAKE", "Unsupported or invalid saved game format");
    return false;
  }
  if (!doc["active"].as<bool>()) return true;

  const int state = doc["state"] | -1;
  const int snakeLen = doc["snakeLen"] | 0;
  const int headIndex = doc["headIndex"] | -1;
  const int foodX = doc["foodX"] | -1;
  const int foodY = doc["foodY"] | -1;
  const int dirX = doc["dirX"] | 0;
  const int dirY = doc["dirY"] | 0;
  const int nextDirX = doc["nextDirX"] | 0;
  const int nextDirY = doc["nextDirY"] | 0;
  const int score = doc["score"] | -1;
  const unsigned long tickIntervalMs = doc["tickIntervalMs"] | 0UL;
  const JsonArrayConst xs = doc["snakeX"].as<JsonArrayConst>();
  const JsonArrayConst ys = doc["snakeY"].as<JsonArrayConst>();

  auto validDir = [](int x, int y) {
    return (x == 1 && y == 0) || (x == -1 && y == 0) || (x == 0 && y == 1) || (x == 0 && y == -1);
  };
  if (state < 0 || state > 3 || snakeLen < 1 || snakeLen > MAX_LEN || headIndex < 0 || headIndex >= MAX_LEN ||
      foodX < 0 || foodX >= GRID_W || foodY < 0 || foodY >= GRID_H || !validDir(dirX, dirY) ||
      !validDir(nextDirX, nextDirY) || score < 0 || tickIntervalMs < 100 || tickIntervalMs > 5000 ||
      xs.size() != MAX_LEN || ys.size() != MAX_LEN) {
    LOG_ERR("SNAKE", "Invalid saved game data");
    return false;
  }

  std::array<Point, MAX_LEN> snake{};
  for (size_t i = 0; i < MAX_LEN; ++i) {
    if (!xs[i].is<int>() || !ys[i].is<int>()) return false;
    const int x = xs[i].as<int>();
    const int y = ys[i].as<int>();
    if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return false;
    snake[i] = Point{static_cast<int8_t>(x), static_cast<int8_t>(y)};
  }

  savedState = static_cast<State>(state);
  savedSnake = snake;
  savedSnakeLen = snakeLen;
  savedHeadIndex = headIndex;
  savedFood = Point{static_cast<int8_t>(foodX), static_cast<int8_t>(foodY)};
  savedDirX = static_cast<int8_t>(dirX);
  savedDirY = static_cast<int8_t>(dirY);
  savedNextDirX = static_cast<int8_t>(nextDirX);
  savedNextDirY = static_cast<int8_t>(nextDirY);
  savedScore = score;
  savedTickIntervalMs = tickIntervalMs;
  savedRngState = doc["rngState"] | 1u;
  savedGame = true;
  return true;
}

bool SnakeProgressStore::saveGame(State state, const std::array<Point, MAX_LEN>& snake, int snakeLen, int headIndex,
                                  Point food, int8_t dirX, int8_t dirY, int8_t nextDirX, int8_t nextDirY, int score,
                                  unsigned long tickIntervalMs, uint32_t rngState) {
  savedState = state;
  savedSnake = snake;
  savedSnakeLen = snakeLen;
  savedHeadIndex = headIndex;
  savedFood = food;
  savedDirX = dirX;
  savedDirY = dirY;
  savedNextDirX = nextDirX;
  savedNextDirY = nextDirY;
  savedScore = score;
  savedTickIntervalMs = tickIntervalMs;
  savedRngState = rngState == 0 ? 1 : rngState;
  savedGame = true;
  if (!saveToFile()) {
    LOG_ERR("SNAKE", "Failed to save game progress");
    return false;
  }
  return true;
}

void SnakeProgressStore::clear() {
  savedGame = false;
  saveToFile();
}
