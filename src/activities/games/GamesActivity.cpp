#include "GamesActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <array>

#include "ConnectFourActivity.h"
#include "chess/ChessActivity.h"
#include "MappedInputManager.h"
#include "MinesweeperActivity.h"
#include "NonogramsActivity.h"
#include "SnakeActivity.h"
#include "SudokuActivity.h"
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

void GamesActivity::openSelected() {
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
}

int GamesActivity::rowHeight() const { return std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20); }

int GamesActivity::listTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + 8;
}

void GamesActivity::loop() {
  if (mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Right ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = static_cast<int>(GameChoice::Count);
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    selectedIndex = (selectedIndex + count - 1) % count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    selectedIndex = (selectedIndex + 1) % count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelected();
    return;
  }

  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const int rh = rowHeight();
    const int top = listTop();
    const int margin = std::max(16, UITheme::getInstance().getMetrics().contentSidePadding);
    if (x >= margin && x < renderer.getScreenWidth() - margin && y >= top) {
      const int idx = (y - top) / rh;
      if (idx >= 0 && idx < count) {
        selectedIndex = idx;
        openSelected();
      }
    }
  }
}

void GamesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, tr(STR_GAMES));

  static const char* names[] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  };
  names[0] = "Chess";
  names[1] = tr(STR_CONNECT_FOUR);
  names[2] = tr(STR_MINESWEEPER);
  names[3] = tr(STR_NONOGRAMS);
  names[4] = tr(STR_SNAKE);
  names[5] = tr(STR_SUDOKU);

  static const uint8_t* icons[] = {
      ChessIcon.data(),      ConnectFourIcon.data(), MinesweeperIcon.data(),
      NonogramsIcon.data(),  SnakeIcon.data(),       SudokuIcon.data(),
  };

  const int rh = rowHeight();
  const int top = listTop();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int count = static_cast<int>(GameChoice::Count);
  for (int i = 0; i < count; ++i) {
    const int y = top + i * rh;
    if (y + rh > renderer.getScreenHeight() - metrics.buttonHintsHeight) break;
    const bool sel = i == selectedIndex;
    const int rowX = margin;
    const int rowW = renderer.getScreenWidth() - margin * 2;
    if (sel) {
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

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
