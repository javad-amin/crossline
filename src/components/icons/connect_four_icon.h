#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> ConnectFourIcon = [] {
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
  const auto disc = [&fillRect, &setPixel](int cx, int cy) {
    fillRect(cx - 1, cy - 1, 3, 3);
    setPixel(cx - 2, cy); setPixel(cx + 2, cy);
    setPixel(cx, cy - 2); setPixel(cx, cy + 2);
  };

  // Frame
  for (int x = 4; x <= 27; ++x) { setPixel(x, 6); setPixel(x, 27); }
  for (int y = 6; y <= 27; ++y) { setPixel(4, y); setPixel(27, y); }
  // Slots grid hints
  for (int y = 10; y <= 24; y += 4) for (int x = 7; x <= 24; ++x) setPixel(x, y);
  for (int x = 7; x <= 24; x += 4) for (int y = 10; y <= 24; ++y) setPixel(x, y);
  // Discs
  disc(9, 22); disc(13, 22); disc(17, 22); disc(21, 18);
  disc(13, 18); disc(17, 14); disc(9, 14);
  // highlight a connect-4 diagonal
  fillRect(20, 20, 2, 2);
  return bitmap;
}();
