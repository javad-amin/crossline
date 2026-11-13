#pragma once

#include <array>
#include <cstdint>

inline constexpr std::array<uint8_t, 128> SudokuIcon = [] {
  std::array<uint8_t, 128> bitmap{};
  bitmap.fill(0xFF);

  const auto setPixel = [&bitmap](const int x, const int y) {
    const int row = 31 - x;
    bitmap[row * 4 + y / 8] &= static_cast<uint8_t>(~(1u << (7 - y % 8)));
  };

  for (int position = 1; position < 31; ++position) {
    setPixel(1, position);
    setPixel(2, position);
    setPixel(29, position);
    setPixel(30, position);
    setPixel(position, 1);
    setPixel(position, 2);
    setPixel(position, 29);
    setPixel(position, 30);
  }
  for (const int boundary : {10, 11, 20, 21}) {
    for (int position = 2; position < 30; ++position) {
      setPixel(boundary, position);
      setPixel(position, boundary);
    }
  }

  constexpr uint8_t DIGITS[9][5] = {
      {0b010, 0b110, 0b010, 0b010, 0b111}, {0b110, 0b001, 0b010, 0b100, 0b111},
      {0b110, 0b001, 0b010, 0b001, 0b110}, {0b101, 0b101, 0b111, 0b001, 0b001},
      {0b111, 0b100, 0b110, 0b001, 0b110}, {0b011, 0b100, 0b110, 0b101, 0b010},
      {0b111, 0b001, 0b010, 0b010, 0b010}, {0b010, 0b101, 0b010, 0b101, 0b010},
      {0b010, 0b101, 0b011, 0b001, 0b110},
  };
  constexpr int CELL_STARTS[3] = {2, 12, 22};
  for (int cell = 0; cell < 9; ++cell) {
    const int x = CELL_STARTS[cell % 3] + 2;
    const int y = CELL_STARTS[cell / 3] + 1;
    for (int row = 0; row < 5; ++row) {
      for (int column = 0; column < 3; ++column) {
        if ((DIGITS[cell][row] & (1u << (2 - column))) != 0) setPixel(x + column, y + row);
      }
    }
  }
  return bitmap;
}();
