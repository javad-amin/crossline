#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> GamesIcon = [] {
  std::array<uint8_t, 128> bitmap{};
  bitmap.fill(0xFF);
  for (int row = 7; row <= 25; ++row) {
    int left = 5;
    int right = 26;
    if (row <= 9) {
      left = 9;
      right = 22;
    } else if (row >= 22) {
      left = row <= 23 ? 7 : 9;
      right = row <= 23 ? 24 : 22;
    }

    for (int column = left; column <= right; ++column) {
      const int index = row * 4 + column / 8;
      bitmap[index] &= static_cast<uint8_t>(~(1u << (7 - column % 8)));
    }
  }

  for (int row = 12; row <= 18; ++row) {
    for (int column = 8; column <= 14; ++column) {
      if ((row >= 14 && row <= 16) || (column >= 10 && column <= 12)) continue;
      const int index = row * 4 + column / 8;
      bitmap[index] |= static_cast<uint8_t>(1u << (7 - column % 8));
    }
  }
  for (int button = 0; button < 4; ++button) {
    const int row = 12 + button * 2;
    for (int side = 0; side < 2; ++side) {
      const int column = 20 + side * 3;
      const int index = row * 4 + column / 8;
      bitmap[index] |= static_cast<uint8_t>(1u << (7 - column % 8));
    }
  }
  return bitmap;
}();
