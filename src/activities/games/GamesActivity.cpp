#include "GamesActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <array>

#include "ConnectFourActivity.h"
#include "MappedInputManager.h"
#include "MinesweeperActivity.h"
#include "NonogramsActivity.h"
#include "SnakeActivity.h"
#include "SudokuActivity.h"
#include "chess/ChessActivity.h"
#include "../apps/CalculatorActivity.h"
#include "../apps/StopwatchActivity.h"
#include "components/UITheme.h"
#include "components/icons/chess_icon.h"
#include "components/icons/connect_four_icon.h"
#include "components/icons/minesweeper_icon.h"
#include "components/icons/nonograms_icon.h"
#include "components/icons/snake_icon.h"
#include "components/icons/sudoku_icon.h"
#include "fontIds.h"

GamesActivity::GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Games", renderer, mappedInput) {}

void GamesActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

int GamesActivity::currentCount() const {
  return activeTab == Tab::Games ? static_cast<int>(GameChoice::Count) : static_cast<int>(AppChoice::Count);
}

void GamesActivity::openSelected() {
  if (activeTab == Tab::Games) {
    switch (static_cast<GameChoice>(selectedIndex)) {
      case GameChoice::Chess: {
        auto activity = makeUniqueNoThrow<ChessActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::ConnectFour: {
        auto activity = makeUniqueNoThrow<ConnectFourActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::Minesweeper: {
        auto activity = makeUniqueNoThrow<MinesweeperActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::Nonograms: {
        auto activity = makeUniqueNoThrow<NonogramsActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::Snake: {
        auto activity = makeUniqueNoThrow<SnakeActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::Sudoku: {
        auto activity = makeUniqueNoThrow<SudokuActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case GameChoice::Count:
        return;
    }
  } else {
    switch (static_cast<AppChoice>(appSelectedIndex)) {
      case AppChoice::Calculator: {
        auto activity = makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case AppChoice::Stopwatch: {
        auto activity = makeUniqueNoThrow<StopwatchActivity>(renderer, mappedInput);
        if (activity) startActivityForResult(std::move(activity), [](const ActivityResult&) {});
        return;
      }
      case AppChoice::Count:
        return;
    }
  }
}

int GamesActivity::tabBarHeight() const { return std::max(36, renderer.getLineHeight(UI_10_FONT_ID) + 16); }

int GamesActivity::tabBarTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight;
}

int GamesActivity::rowHeight() const { return std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20); }

int GamesActivity::listTop() const { return tabBarTop() + tabBarHeight() + 8; }

void GamesActivity::drawTabs() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = tabBarTop();
  const int h = tabBarHeight();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int w = renderer.getScreenWidth() - margin * 2;
  const int half = w / 2;
  const char* labels[] = {tr(STR_GAMES), "Apps"};
  for (int i = 0; i < 2; ++i) {
    const int x = margin + i * half;
    const bool active = static_cast<int>(activeTab) == i;
    if (active) {
      renderer.fillRect(x, top, half, h);
    } else {
      renderer.drawRect(x, top, half, h);
    }
    const int ty = top + (h - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_12_FONT_ID, ty, labels[i], !active);
  }
  renderer.fillRect(margin, top + h - 2, w, 2);
}

void GamesActivity::loop() {
  const bool onGames = activeTab == Tab::Games;
  int& sel = onGames ? selectedIndex : appSelectedIndex;

  if (mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Right ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = currentCount();
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    sel = (sel + count - 1) % count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    sel = (sel + 1) % count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (activeTab == Tab::Apps) {
      activeTab = Tab::Games;
      requestUpdate();
    } else {
      sel = (sel + count - 1) % count;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (activeTab == Tab::Games) {
      activeTab = Tab::Apps;
      requestUpdate();
    } else {
      sel = (sel + 1) % count;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelected();
    return;
  }

  // Page buttons can switch tabs quickly too
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    activeTab = Tab::Games;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    activeTab = Tab::Apps;
    requestUpdate();
    return;
  }

  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int margin = std::max(16, metrics.contentSidePadding);
    const int w = renderer.getScreenWidth() - margin * 2;
    const int half = w / 2;
    const int tTop = tabBarTop();
    const int tH = tabBarHeight();
    if (y >= tTop && y < tTop + tH && x >= margin && x < margin + w) {
      activeTab = (x < margin + half) ? Tab::Games : Tab::Apps;
      requestUpdate();
      return;
    }
    const int rh = rowHeight();
    const int top = listTop();
    if (x >= margin && x < renderer.getScreenWidth() - margin && y >= top) {
      const int idx = (y - top) / rh;
      if (idx >= 0 && idx < count) {
        sel = idx;
        openSelected();
      }
    }
  }
}

void GamesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_GAMES));
  drawTabs();

  const int rh = rowHeight();
  const int top = listTop();
  const int margin = std::max(16, metrics.contentSidePadding);
  const bool onGames = activeTab == Tab::Games;
  const int count = currentCount();
  const int sel = onGames ? selectedIndex : appSelectedIndex;

  if (onGames) {
    static const char* names[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    names[0] = "Chess";
    names[1] = tr(STR_CONNECT_FOUR);
    names[2] = tr(STR_MINESWEEPER);
    names[3] = tr(STR_NONOGRAMS);
    names[4] = tr(STR_SNAKE);
    names[5] = tr(STR_SUDOKU);
    static const uint8_t* icons[] = {ChessIcon.data(),      ConnectFourIcon.data(), MinesweeperIcon.data(),
                                     NonogramsIcon.data(), SnakeIcon.data(),       SudokuIcon.data()};
    for (int i = 0; i < count; ++i) {
      const int y = top + i * rh;
      if (y + rh > renderer.getScreenHeight() - metrics.buttonHintsHeight) break;
      const bool selected = i == sel;
      const int rowX = margin;
      const int rowW = renderer.getScreenWidth() - margin * 2;
      if (selected) {
        renderer.drawRect(rowX, y, rowW, rh - 4, 2, true);
        renderer.fillRect(rowX + 3, y + 3, 4, rh - 10);
      } else {
        renderer.drawRect(rowX, y, rowW, rh - 4);
      }
      const int iconSize = 32;
      const int iconX = rowX + 14;
      const int iconY = y + (rh - 4 - iconSize) / 2;
      renderer.drawIcon(icons[i], iconX, iconY, iconSize);
      const int textX = iconX + iconSize + 12;
      const int textY = y + (rh - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, textX, textY, names[i], true);
    }
  } else {
    static const char* appNames[] = {"Calculator", "Stopwatch"};
    for (int i = 0; i < count; ++i) {
      const int y = top + i * rh;
      if (y + rh > renderer.getScreenHeight() - metrics.buttonHintsHeight) break;
      const bool selected = i == sel;
      const int rowX = margin;
      const int rowW = renderer.getScreenWidth() - margin * 2;
      if (selected) {
        renderer.drawRect(rowX, y, rowW, rh - 4, 2, true);
        renderer.fillRect(rowX + 3, y + 3, 4, rh - 10);
      } else {
        renderer.drawRect(rowX, y, rowW, rh - 4);
      }
      const int textY = y + (rh - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
      renderer.drawText(UI_12_FONT_ID, rowX + 16, textY, appNames[i], true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
