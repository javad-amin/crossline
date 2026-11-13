#include <GfxRenderer.h>
#pragma once

#include <string>
#include "activities/Activity.h"

class CalculatorActivity final : public Activity {
 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class KeyId : uint8_t {
    C, CE, Back, Div,
    K7, K8, K9, Mul,
    K4, K5, K6, Sub,
    K1, K2, K3, Add,
    Sign, K0, Dot, Eq,
    Count
  };

  void press(KeyId key);
  void pressDigit(int d);
  void pressOp(char op);
  void calculate();
  void clearAll();
  void clearEntry();
  void backspace();
  void updateDisplay();
  std::string formatNumber(double v) const;

  std::string display = "0";
  std::string pendingDisplay;
  double accumulator = 0.0;
  double current = 0.0;
  char pendingOp = 0; // + - * /
  bool entering = false;
  bool justEvaluated = false;
  bool error = false;

  int cursor = 4; // start on 7
  bool needsRedraw = true;

  void moveCursor(int dx, int dy);
  KeyId keyAtCursor() const;
  void handleKey(KeyId key);
  int keyIndexAtPoint(int x, int y) const;
  void drawKeypad() const;
  void keypadRect(int& x, int& y, int& w, int& h) const;
};
