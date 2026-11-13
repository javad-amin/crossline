#include "SnakeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "SnakeProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

SnakeActivity::SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Snake", renderer, mappedInput) {}

void SnakeActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis());
  if (rngState == 0) rngState = 1;
  auto& progress = SnakeProgressStore::getInstance();
  if (progress.loadFromFile() && progress.hasSavedGame()) {
    hasSavedGame = true;
    state = static_cast<State>(progress.state());
    snakeLen = progress.snakeLen();
    headIndex = progress.headIndex();
    food = Point{progress.food().x, progress.food().y};
    dirX = progress.dirX();
    dirY = progress.dirY();
    nextDirX = progress.nextDirX();
    nextDirY = progress.nextDirY();
    score = progress.score();
    tickIntervalMs = progress.tickIntervalMs();
    rngState = progress.rngState();
    for (int i = 0; i < MAX_LEN; ++i) {
      snake[i] = Point{progress.snake()[i].x, progress.snake()[i].y};
    }
    screen = Screen::StartMenu;
    startSelection = 0;
  } else {
    resetGame();
    screen = Screen::Playing;
  }
  requestUpdate();
}

void SnakeActivity::continueGame() {
  screen = Screen::Playing;
  if (state == State::Playing) {
    lastTickMs = millis();
  }
  requestUpdate();
}

void SnakeActivity::saveProgress() const {
  std::array<SnakeProgressStore::Point, SnakeProgressStore::MAX_LEN> storeSnake{};
  for (int i = 0; i < MAX_LEN; ++i) {
    storeSnake[i] = SnakeProgressStore::Point{snake[i].x, snake[i].y};
  }
  SnakeProgressStore::getInstance().saveGame(static_cast<SnakeProgressStore::State>(state), storeSnake, snakeLen,
                                             headIndex, SnakeProgressStore::Point{food.x, food.y}, dirX, dirY, nextDirX,
                                             nextDirY, score, tickIntervalMs, rngState);
}

int SnakeActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

void SnakeActivity::resetGame() {
  state = State::Ready;
  snakeLen = 3;
  headIndex = snakeLen - 1;
  const int startX = GRID_W / 2 - 1;
  const int startY = GRID_H / 2;
  for (int i = 0; i < snakeLen; ++i) {
    snake[i] = Point{static_cast<int8_t>(startX - (snakeLen - 1 - i)), static_cast<int8_t>(startY)};
  }
  dirX = 1;
  dirY = 0;
  nextDirX = 1;
  nextDirY = 0;
  score = 0;
  tickIntervalMs = 650;
  lastTickMs = millis();
  placeFood();
  hasSavedGame = true;
  saveProgress();
}

bool SnakeActivity::isOpposite(const int8_t dx, const int8_t dy) const {
  return dx == -dirX && dy == -dirY;
}

void SnakeActivity::setDirection(const int8_t dx, const int8_t dy) {
  if (snakeLen > 1 && isOpposite(dx, dy)) return;
  nextDirX = dx;
  nextDirY = dy;
}

bool SnakeActivity::occupies(const Point& p, const bool includeTail) const {
  const int count = includeTail ? snakeLen : (snakeLen > 0 ? snakeLen - 1 : 0);
  // snake stored as ring ending at headIndex, oldest at (headIndex - snakeLen + 1)
  for (int i = 0; i < count; ++i) {
    const int idx = (headIndex - (snakeLen - 1) + i + MAX_LEN) % MAX_LEN;
    if (snake[idx] == p) return true;
  }
  return false;
}

void SnakeActivity::placeFood() {
  // Simple LCG for deterministic-enough placement without extra deps.
  auto nextRand = [this]() -> uint32_t {
    rngState = rngState * 1664525u + 1013904223u;
    return rngState;
  };
  for (int tries = 0; tries < 200; ++tries) {
    const Point p{static_cast<int8_t>(nextRand() % GRID_W), static_cast<int8_t>(nextRand() % GRID_H)};
    bool onSnake = false;
    for (int i = 0; i < snakeLen; ++i) {
      const int idx = (headIndex - (snakeLen - 1) + i + MAX_LEN) % MAX_LEN;
      if (snake[idx] == p) {
        onSnake = true;
        break;
      }
    }
    if (!onSnake) {
      food = p;
      return;
    }
  }
  // fallback (board nearly full)
  for (int y = 0; y < GRID_H; ++y) {
    for (int x = 0; x < GRID_W; ++x) {
      Point p{static_cast<int8_t>(x), static_cast<int8_t>(y)};
      bool onSnake = false;
      for (int i = 0; i < snakeLen; ++i) {
        const int idx = (headIndex - (snakeLen - 1) + i + MAX_LEN) % MAX_LEN;
        if (snake[idx] == p) {
          onSnake = true;
          break;
        }
      }
      if (!onSnake) {
        food = p;
        return;
      }
    }
  }
}

void SnakeActivity::step() {
  if (state != State::Playing) return;
  dirX = nextDirX;
  dirY = nextDirY;

  const int curHead = headIndex;
  const Point head = snake[curHead];
  const Point next{static_cast<int8_t>(head.x + dirX), static_cast<int8_t>(head.y + dirY)};

  // wall collision
  if (next.x < 0 || next.x >= GRID_W || next.y < 0 || next.y >= GRID_H) {
    state = State::GameOver;
    saveProgress();
    requestUpdate();
    return;
  }

  const bool grow = (next == food);
  // self collision (tail moves unless growing)
  if (occupies(next, grow)) {
    state = State::GameOver;
    saveProgress();
    requestUpdate();
    return;
  }

  headIndex = (headIndex + 1) % MAX_LEN;
  snake[headIndex] = next;
  if (grow) {
    if (snakeLen < MAX_LEN) ++snakeLen;
    ++score;
    if (tickIntervalMs > 350) tickIntervalMs -= 10;
    placeFood();
  }

  saveProgress();
  requestUpdate();
}

void SnakeActivity::loop() {
  if (screen == Screen::StartMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      startSelection = 1 - startSelection;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (startSelection == 0) {
        continueGame();
      } else {
        screen = Screen::Playing;
        resetGame();
        requestUpdate();
      }
      return;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
      const int top = startMenuTop();
      const int idx = (ty - top) / rowHeight;
      const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
      if (tx >= margin && tx < renderer.getScreenWidth() - margin && idx >= 0 && idx < 2 && ty >= top) {
        startSelection = idx;
        if (startSelection == 0) {
          continueGame();
        } else {
          screen = Screen::Playing;
          resetGame();
          requestUpdate();
        }
      }
    }
    return;
  }

  // Global exits
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveProgress();
    finish();
    return;
  }

  if (state == State::Ready || state == State::GameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      resetGame();
      state = State::Playing;
      lastTickMs = millis();
      saveProgress();
      requestUpdate();
      return;
    }
  }

  if (state == State::Paused) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = State::Playing;
      lastTickMs = millis();
      saveProgress();
      requestUpdate();
      return;
    }
  } else if (state == State::Playing) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = State::Paused;
      saveProgress();
      requestUpdate();
      return;
    }
  }

  bool stateChanged = false;
  // Direction input
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    if (state == State::Ready) {
      state = State::Playing;
      lastTickMs = millis();
      stateChanged = true;
    }
    setDirection(0, -1);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    if (state == State::Ready) {
      state = State::Playing;
      lastTickMs = millis();
      stateChanged = true;
    }
    setDirection(0, 1);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (state == State::Ready) {
      state = State::Playing;
      lastTickMs = millis();
      stateChanged = true;
    }
    setDirection(-1, 0);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (state == State::Ready) {
      state = State::Playing;
      lastTickMs = millis();
      stateChanged = true;
    }
    setDirection(1, 0);
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) { if (state == State::Ready) { state = State::Playing; lastTickMs = millis(); stateChanged = true; } setDirection(0, -1); }
  else if (swipe == MappedInputManager::SwipeDir::Down) { if (state == State::Ready) { state = State::Playing; lastTickMs = millis(); stateChanged = true; } setDirection(0, 1); }
  else if (swipe == MappedInputManager::SwipeDir::Left) { if (state == State::Ready) { state = State::Playing; lastTickMs = millis(); stateChanged = true; } setDirection(-1, 0); }
  else if (swipe == MappedInputManager::SwipeDir::Right) { if (state == State::Ready) { state = State::Playing; lastTickMs = millis(); stateChanged = true; } setDirection(1, 0); }

  if (stateChanged) saveProgress();

  if (state == State::Playing) {
    const unsigned long now = millis();
    if (now - lastTickMs >= tickIntervalMs) {
      lastTickMs = now;
      step();
    }
  }
}

void SnakeActivity::drawStartMenu() const {
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int top = startMenuTop();
  const char* labels[] = {tr(STR_CONTINUE), tr(STR_NEW_GAME)};
  const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
  for (int i = 0; i < 2; ++i) {
    const int y = top + i * rowHeight;
    const bool selected = startSelection == i;
    if (selected) {
      renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    } else {
      renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, labels[i],
                              !selected);
  }
}

void SnakeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_SNAKE));

  if (screen == Screen::StartMenu) {
    drawStartMenu();
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  char scoreText[48];
  snprintf(scoreText, sizeof(scoreText), "%s: %d", tr(STR_SNAKE_SCORE), score);
  renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 4, scoreText);

  drawBoard();

  const int centerY = renderer.getScreenHeight() / 2 + 40;
  if (state == State::Ready) {
    renderer.drawCenteredText(UI_12_FONT_ID, centerY, tr(STR_SNAKE_READY));
  } else if (state == State::Paused) {
    renderer.drawCenteredText(UI_12_FONT_ID, centerY, tr(STR_SNAKE_PAUSED));
  } else if (state == State::GameOver) {
    renderer.drawCenteredText(UI_12_FONT_ID, centerY - 14, tr(STR_SNAKE_GAME_OVER));
    renderer.drawCenteredText(UI_10_FONT_ID, centerY + 14, tr(STR_SNAKE_NEW_GAME));
  }

  const char* confirmHint = (state == State::Playing) ? tr(STR_PAUSE) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmHint, tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void SnakeActivity::drawBoard() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int topSafe = metrics.topPadding + metrics.headerHeight + 28;
  const int bottomSafe = metrics.buttonHintsHeight + 8;
  const int availW = renderer.getScreenWidth() - 24;
  const int availH = renderer.getScreenHeight() - topSafe - bottomSafe;
  const int cell = std::max(8, std::min(availW / GRID_W, availH / GRID_H));
  const int boardW = cell * GRID_W;
  const int boardH = cell * GRID_H;
  const int left = (renderer.getScreenWidth() - boardW) / 2;
  const int top = topSafe + std::max(0, (availH - boardH) / 2);

  renderer.drawRect(left - 2, top - 2, boardW + 4, boardH + 4, 2, true);

  // food
  const int fx = left + food.x * cell;
  const int fy = top + food.y * cell;
  renderer.fillRect(fx + 2, fy + 2, std::max(2, cell - 4), std::max(2, cell - 4));

  // snake
  for (int i = 0; i < snakeLen; ++i) {
    const int idx = (headIndex - (snakeLen - 1) + i + MAX_LEN) % MAX_LEN;
    const Point p = snake[idx];
    const int x = left + p.x * cell;
    const int y = top + p.y * cell;
    if (i == snakeLen - 1) {
      renderer.fillRect(x + 1, y + 1, cell - 2, cell - 2); // head
    } else {
      renderer.drawRect(x + 1, y + 1, cell - 2, cell - 2, 1, true);
      renderer.fillRectDither(x + 2, y + 2, std::max(1, cell - 4), std::max(1, cell - 4), Color::LightGray);
    }
  }
}
