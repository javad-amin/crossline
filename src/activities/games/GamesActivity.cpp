#include "GamesActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "SudokuActivity.h"
#include "components/UITheme.h"
#include "components/icons/sudoku_icon.h"
#include "fontIds.h"

GamesActivity::GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Games", renderer, mappedInput) {}

void GamesActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void GamesActivity::openSudoku() {
  auto activity = makeUniqueNoThrow<SudokuActivity>(renderer, mappedInput);
  if (!activity) {
    LOG_ERR("GAMES", "OOM: Sudoku activity");
    return;
  }
  startActivityForResult(std::move(activity), [](const ActivityResult&) {});
}

void GamesActivity::loop() {
  if (mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Right) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSudoku();
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y) && x >= tileLeft() && x < tileLeft() + tileWidth() && y >= tileTop() &&
      y < tileTop() + tileHeight()) {
    openSudoku();
  }
}

int GamesActivity::tileWidth() const { return std::min(180, renderer.getScreenWidth() - 48); }

int GamesActivity::tileHeight() const { return 160; }

int GamesActivity::tileLeft() const { return (renderer.getScreenWidth() - tileWidth()) / 2; }

int GamesActivity::tileTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - tileHeight()) / 2);
}

void GamesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_GAMES));

  const int x = tileLeft();
  const int y = tileTop();
  const int width = tileWidth();
  const int height = tileHeight();
  renderer.drawRect(x, y, width, height, 3, true);
  const int iconSize = 32;
  renderer.drawIcon(SudokuIcon.data(), (renderer.getScreenWidth() - iconSize) / 2, y + 28, iconSize);
  renderer.drawCenteredText(UI_12_FONT_ID, y + 104, tr(STR_SUDOKU));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
