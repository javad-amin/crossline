#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> NonogramsIcon = [] {
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

  // Outer frame
  for (int i = 6; i <= 27; ++i) { setPixel(6, i); setPixel(27, i); setPixel(i, 6); setPixel(i, 27); }
  // 5x5 grid
  for (int k = 0; k <= 5; ++k) {
    const int p = 6 + k * 4 + (k == 0 ? 0 : 1);
    // approximate divisions
  }
  for (int i = 10; i <= 23; i += 4) { for (int j = 6; j <= 27; ++j) { setPixel(i, j); setPixel(j, i); } }
  // Clue bars top/left
  fillRect(11, 3, 2, 2); fillRect(15, 2, 2, 3); fillRect(19, 3, 2, 2); fillRect(23, 2, 2, 3);
  fillRect(3, 11, 2, 2); fillRect(2, 15, 3, 2); fillRect(3, 19, 2, 2); fillRect(2, 23, 3, 2);
  // Filled cells
  fillRect(7, 7, 3, 3); fillRect(15, 7, 3, 3); fillRect(23, 11, 3, 3);
  fillRect(11, 15, 3, 3); fillRect(19, 15, 3, 3); fillRect(7, 19, 3, 3);
  fillRect(15, 23, 3, 3); fillRect(23, 23, 3, 3);
  return bitmap;
}();
