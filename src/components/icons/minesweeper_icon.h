#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> MinesweeperIcon = [] {
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
  // grid
  for (int i = 4; i <= 27; ++i) { setPixel(4,i); setPixel(27,i); setPixel(i,4); setPixel(i,27); }
  for (int i = 12; i <= 19; ++i) { setPixel(12,i); setPixel(19,i); setPixel(i,12); setPixel(i,19); }
  // mine
  fillRect(14,14,4,4);
  setPixel(13,15); setPixel(13,16); setPixel(18,15); setPixel(18,16);
  setPixel(15,13); setPixel(16,13); setPixel(15,18); setPixel(16,18);
  // flag
  fillRect(6,6,2,4); setPixel(8,6); setPixel(9,7); setPixel(8,8);
  // number-like marks
  setPixel(22,7); setPixel(23,6); setPixel(24,7); setPixel(23,8);
  return bitmap;
}();
