#include "StopwatchActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

StopwatchActivity::StopwatchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Stopwatch", renderer, mappedInput) {}

void StopwatchActivity::onEnter() {
  Activity::onEnter();
  running = false;
  elapsedMs = 0;
  lastTickMs = millis();
  lapCount = 0;
  selectedAction = 0;
  needsRedraw = true;
  requestUpdate();
}

void StopwatchActivity::formatTime(char* out, size_t outSize, uint32_t ms) const {
  const uint32_t totalSec = ms / 1000;
  const uint32_t minutes = totalSec / 60;
  const uint32_t seconds = totalSec % 60;
  const uint32_t centis = (ms % 1000) / 10;
  snprintf(out, outSize, "%02lu:%02lu.%02lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds), static_cast<unsigned long>(centis));
}

void StopwatchActivity::toggleRun() {
  if (!running) {
    running = true;
    lastTickMs = millis();
  } else {
    running = false;
  }
  needsRedraw = true;
}

void StopwatchActivity::lapOrReset() {
  if (running) {
    if (lapCount < static_cast<int>(laps.size())) {
      for (int i = lapCount; i > 0; --i) laps[i] = laps[i - 1];
      laps[0] = elapsedMs;
      lapCount++;
    }
  } else {
    elapsedMs = 0;
    lapCount = 0;
  }
  needsRedraw = true;
}

void StopwatchActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selectedAction = 1 - selectedAction;
    needsRedraw = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectedAction == 0) toggleRun(); else lapOrReset();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) { selectedAction = 0; toggleRun(); }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) { selectedAction = 1; lapOrReset(); }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int midY = renderer.getScreenHeight() / 2;
    const int rowH = std::max(48, renderer.getLineHeight(UI_12_FONT_ID) + 24);
    const int top = midY + 20;
    const int margin = std::max(20, metrics.contentSidePadding);
    if (tx >= margin && tx < renderer.getScreenWidth() - margin && ty >= top && ty < top + rowH * 2) {
      selectedAction = (ty - top) / rowH;
      if (selectedAction == 0) toggleRun(); else lapOrReset();
    }
  }

  if (running) {
    const uint32_t now = millis();
    elapsedMs += (now - lastTickMs);
    lastTickMs = now;
    needsRedraw = true;
  }

  if (needsRedraw) {
    needsRedraw = false;
    requestUpdate();
  }
}

void StopwatchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Stopwatch");

  char timeBuf[24];
  formatTime(timeBuf, sizeof(timeBuf), elapsedMs);
  const int centerY = renderer.getScreenHeight() / 2 - 40;
  renderer.drawCenteredText(UI_12_FONT_ID, centerY - 40, running ? "Running" : "Stopped");
  renderer.drawCenteredText(UI_12_FONT_ID, centerY, timeBuf);

  const char* actions[] = {running ? "Stop" : "Start", running ? "Lap" : "Reset"};
  const int rowH = std::max(48, renderer.getLineHeight(UI_12_FONT_ID) + 24);
  const int top = centerY + 40;
  const int margin = std::max(20, metrics.contentSidePadding);
  for (int i = 0; i < 2; ++i) {
    const int y = top + i * rowH;
    const bool sel = i == selectedAction;
    if (sel) renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    else renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowH - 4);
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowH - renderer.getLineHeight(UI_12_FONT_ID)) / 2, actions[i], !sel);
  }

  int lapY = top + rowH * 2 + 16;
  for (int i = 0; i < lapCount && i < 5; ++i) {
    char buf[40];
    char t[24];
    formatTime(t, sizeof(t), laps[i]);
    snprintf(buf, sizeof(buf), "Lap %d: %s", lapCount - i, t);
    renderer.drawCenteredText(SMALL_FONT_ID, lapY, buf);
    lapY += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), running ? "Start/Stop" : "Start/Stop", running ? "Lap" : "Reset");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
