#include "CalculatorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct KeyDef { const char* label; };
const KeyDef kKeys[20] = {
  {"C"}, {"CE"}, {"<"}, {"/"},
  {"7"}, {"8"}, {"9"}, {"*"},
  {"4"}, {"5"}, {"6"}, {"-"},
  {"1"}, {"2"}, {"3"}, {"+"},
  {"+/-"}, {"0"}, {"."}, {"="},
};
}

CalculatorActivity::CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Calculator", renderer, mappedInput) {}

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  clearAll();
  needsRedraw = true;
  requestUpdate();
}

void CalculatorActivity::clearAll() {
  display = "0";
  pendingDisplay.clear();
  accumulator = 0.0;
  current = 0.0;
  pendingOp = 0;
  entering = false;
  justEvaluated = false;
  error = false;
}

void CalculatorActivity::clearEntry() {
  if (error) { clearAll(); return; }
  display = "0";
  current = 0.0;
  entering = false;
}

void CalculatorActivity::updateDisplay() {
  // keep display as-is when entering text; for results format
}

std::string CalculatorActivity::formatNumber(double v) const {
  if (!std::isfinite(v)) return "Error";
  // if integer-ish, show integer
  double iv;
  if (std::fabs(v) <= 9e15 && std::modf(v, &iv) == 0.0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f", v);
    return buf;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "%.10g", v);
  return buf;
}

void CalculatorActivity::backspace() {
  if (error) { clearAll(); return; }
  if (!entering) return;
  if (display.size() > 1) display.pop_back();
  else display = "0";
  if (display == "-" || display.empty()) display = "0";
  try { current = std::stod(display); } catch (...) { current = 0.0; display = "0"; }
}

void CalculatorActivity::pressDigit(int d) {
  if (error) clearAll();
  if (justEvaluated) { pendingOp = 0; pendingDisplay.clear(); display = "0"; justEvaluated = false; }
  if (!entering || display == "0") { display = ""; entering = true; }
  if (display.size() < 16) display += char('0' + d);
  try { current = std::stod(display); } catch (...) {}
}

void CalculatorActivity::calculate() {
  if (!pendingOp) return;
  double result = accumulator;
  if (pendingOp == '+') result = accumulator + current;
  else if (pendingOp == '-') result = accumulator - current;
  else if (pendingOp == '*') result = accumulator * current;
  else if (pendingOp == '/') {
    if (current == 0.0) { error = true; display = "Error"; pendingDisplay.clear(); pendingOp = 0; entering = false; return; }
    result = accumulator / current;
  }
  accumulator = result;
  display = formatNumber(result);
  if (display == "Error" || display == "inf" || display == "-inf" || display == "nan") { error = true; display = "Error"; }
  current = accumulator;
  entering = false;
}

void CalculatorActivity::pressOp(char op) {
  if (error) return;
  if (!entering && !justEvaluated && pendingOp) { pendingOp = op; }
  else {
    if (pendingOp && entering) {
      calculate();
      if (error) return;
    } else {
      try { accumulator = entering ? std::stod(display) : current; } catch (...) { accumulator = current; }
    }
    pendingOp = op;
  }
  justEvaluated = false;
  entering = false;
  char buf[64];
  snprintf(buf, sizeof(buf), "%s %c", formatNumber(accumulator).c_str(), op);
  pendingDisplay = buf;
  current = accumulator;
}

void CalculatorActivity::handleKey(KeyId key) {
  if (error && key != KeyId::C && key != KeyId::CE) return;
  switch (key) {
    case KeyId::C: clearAll(); break;
    case KeyId::CE: clearEntry(); break;
    case KeyId::Back: backspace(); break;
    case KeyId::Div: pressOp('/'); break;
    case KeyId::K7: pressDigit(7); break;
    case KeyId::K8: pressDigit(8); break;
    case KeyId::K9: pressDigit(9); break;
    case KeyId::Mul: pressOp('*'); break;
    case KeyId::K4: pressDigit(4); break;
    case KeyId::K5: pressDigit(5); break;
    case KeyId::K6: pressDigit(6); break;
    case KeyId::Sub: pressOp('-'); break;
    case KeyId::K1: pressDigit(1); break;
    case KeyId::K2: pressDigit(2); break;
    case KeyId::K3: pressDigit(3); break;
    case KeyId::Add: pressOp('+'); break;
    case KeyId::Sign:
      if (!error) {
        if (entering || justEvaluated) {
          if (!display.empty() && display[0] == '-') display.erase(0,1);
          else if (display != "0") display = "-" + display;
          try { current = std::stod(display); } catch (...) {}
        } else { current = -current; accumulator = current; display = formatNumber(current); }
      }
      break;
    case KeyId::K0: pressDigit(0); break;
    case KeyId::Dot:
      if (error) clearAll();
      if (justEvaluated) { pendingOp = 0; pendingDisplay.clear(); display = "0"; justEvaluated = false; }
      if (!entering) { display = "0"; entering = true; }
      if (display.find('.') == std::string::npos) display += '.';
      break;
    case KeyId::Eq:
      if (error) break;
      if (pendingOp) {
        if (!entering) current = accumulator;
        else { try { current = std::stod(display); } catch (...) {} }
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %c %s =", formatNumber(accumulator).c_str(), pendingOp, formatNumber(current).c_str());
        pendingDisplay = buf;
        calculate();
        pendingOp = 0;
        justEvaluated = true;
      } else {
        try { current = std::stod(display); } catch (...) {}
        display = formatNumber(current);
        justEvaluated = true;
      }
      break;
    default: break;
  }
  needsRedraw = true;
}

void CalculatorActivity::moveCursor(int dx, int dy) {
  int col = cursor % 4;
  int row = cursor / 4;
  col = (col + dx + 4) % 4;
  row = (row + dy + 5) % 5;
  cursor = row * 4 + col;
}

CalculatorActivity::KeyId CalculatorActivity::keyAtCursor() const { return static_cast<KeyId>(cursor); }

void CalculatorActivity::keypadRect(int& x, int& y, int& w, int& h) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int top = metrics.topPadding + metrics.headerHeight + 90;
  const int bottom = sh - metrics.buttonHintsHeight - 8;
  const int left = std::max(8, metrics.contentSidePadding);
  const int right = sw - left;
  x = left; y = top; w = right - left; h = std::max(100, bottom - top);
}

int CalculatorActivity::keyIndexAtPoint(int px, int py) const {
  int rx, ry, rw, rh;
  keypadRect(rx, ry, rw, rh);
  if (px < rx || py < ry || px >= rx + rw || py >= ry + rh) return -1;
  const int cols = 4, rows = 5;
  const int cellW = rw / cols;
  const int cellH = rh / rows;
  int col = (px - rx) / cellW;
  int row = (py - ry) / cellH;
  col = std::clamp(col, 0, cols - 1);
  row = std::clamp(row, 0, rows - 1);
  return row * cols + col;
}

void CalculatorActivity::drawKeypad() const {
  int rx, ry, rw, rh;
  keypadRect(rx, ry, rw, rh);
  const int cols = 4, rows = 5;
  const int cellW = rw / cols;
  const int cellH = rh / rows;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const int idx = row * cols + col;
      const int x = rx + col * cellW;
      const int y = ry + row * cellH;
      const bool sel = (idx == cursor);
      if (sel) renderer.fillRect(x + 2, y + 2, cellW - 4, cellH - 4);
      else renderer.drawRect(x + 2, y + 2, cellW - 4, cellH - 4);
      const char* label = kKeys[idx].label;
      const int font = UI_12_FONT_ID;
      const int tw = renderer.getTextWidth(font, label);
      const int th = renderer.getLineHeight(font);
      renderer.drawText(font, x + (cellW - tw) / 2, y + (cellH - th) / 2, label, !sel);
    }
  }
}

void CalculatorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { moveCursor(0, -1); needsRedraw = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { moveCursor(0, 1); needsRedraw = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) { moveCursor(-1, 0); needsRedraw = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) { moveCursor(1, 0); needsRedraw = true; }
  else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { handleKey(keyAtCursor()); }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    const int idx = keyIndexAtPoint(tx, ty);
    if (idx >= 0) { cursor = idx; handleKey(static_cast<KeyId>(idx)); }
  }

  if (needsRedraw) { needsRedraw = false; requestUpdate(); }
}

void CalculatorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Calculator");

  const int margin = std::max(12, metrics.contentSidePadding);
  const int dispTop = metrics.topPadding + metrics.headerHeight + 8;
  const int dispH = 72;
  renderer.drawRect(margin, dispTop, renderer.getScreenWidth() - margin * 2, dispH, 2, true);
  if (!pendingDisplay.empty()) {
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, pendingDisplay.c_str());
    renderer.drawText(SMALL_FONT_ID, renderer.getScreenWidth() - margin - tw - 8, dispTop + 6, pendingDisplay.c_str());
  }
  const std::string& main = display;
  const int font = UI_12_FONT_ID;
  const int tw = renderer.getTextWidth(font, main.c_str());
  const int th = renderer.getLineHeight(font);
  renderer.drawText(font, renderer.getScreenWidth() - margin - tw - 10, dispTop + dispH - th - 8, main.c_str());

  drawKeypad();

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
