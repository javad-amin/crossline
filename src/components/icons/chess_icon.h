#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> ChessIcon = [] {
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

  // Base
  fillRect(8, 24, 16, 3);
  fillRect(10, 22, 12, 2);
  // Body
  fillRect(12, 14, 8, 8);
  fillRect(11, 16, 2, 4);
  fillRect(19, 16, 2, 4);
  // Neck
  fillRect(13, 11, 6, 3);
  // Knight head
  fillRect(12, 6, 9, 5);
  fillRect(20, 7, 2, 3); // snout
  fillRect(13, 4, 4, 2); // ear/mane
  setPixel(12, 8); setPixel(11, 9);
  // Eye (clear pixel in dark head) - approximate by skipping; draw mane detail
  fillRect(14, 9, 1, 1);
  // Mane back
  for (int y = 7; y <= 12; ++y) setPixel(11, y);
  setPixel(12, 12);

  return bitmap;
}();
