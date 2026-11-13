#pragma once

#include <array>
#include <cstdint>

// 32x32 1-bit icon bitmap matching the existing Games/Sudoku icon format.
inline constexpr std::array<uint8_t, 128> SnakeIcon = [] {
  std::array<uint8_t, 128> bitmap{};
  bitmap.fill(0xFF);

  const auto setPixel = [&bitmap](const int x, const int y) {
    if (x < 0 || x >= 32 || y < 0 || y >= 32) return;
    const int row = 31 - x;
    bitmap[row * 4 + y / 8] &= static_cast<uint8_t>(~(1u << (7 - y % 8)));
  };
  const auto fillRect = [&setPixel](int x, int y, int w, int h) {
    for (int dx = 0; dx < w; ++dx)
      for (int dy = 0; dy < h; ++dy) setPixel(x + dx, y + dy);
  };

  // Border
  for (int i = 2; i <= 29; ++i) {
    setPixel(2, i); setPixel(29, i); setPixel(i, 2); setPixel(i, 29);
  }
  // Snake body (L shape)
  for (int y = 9; y <= 22; ++y) fillRect(10, y, 3, 1);
  for (int x = 10; x <= 20; ++x) fillRect(x, 20, 1, 3);
  // Head
  fillRect(18, 20, 4, 3);
  setPixel(21, 21); // eye highlight as empty? on 1-bit easier to leave; skip
  // Food
  fillRect(22, 8, 3, 3);

  return bitmap;
}();
