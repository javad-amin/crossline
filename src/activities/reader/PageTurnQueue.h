#pragma once

#include <cstdint>

// Page turns pressed while a page is still drawing: presses add up (opposite
// directions cancel) and are applied as one jump once the reader is free.
class PageTurnQueue {
 public:
  static constexpr int8_t MAX_PENDING = 8;

  void add(const bool forward) {
    if (forward && pending < MAX_PENDING) pending++;
    if (!forward && pending > -MAX_PENDING) pending--;
  }
  bool empty() const { return pending == 0; }
  int8_t take() {
    const int8_t turns = pending;
    pending = 0;
    return turns;
  }
  void clear() { pending = 0; }

 private:
  int8_t pending = 0;
};
