#pragma once

#include <array>
#include <cstdint>

namespace sudoku {

constexpr int SIDE = 9;
constexpr int CELL_COUNT = SIDE * SIDE;
constexpr uint16_t ALL_DIGITS = 0x03FE;
using Board = std::array<uint8_t, CELL_COUNT>;
using Notes = std::array<uint16_t, CELL_COUNT>;

enum class Difficulty : uint8_t { Easy, Normal, Hard, Expert, Dev };

struct Puzzle {
  Board givens{};
  Board solution{};
};

inline uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

inline void shuffle(uint8_t* values, const int count, uint32_t& state) {
  for (int i = count - 1; i > 0; --i) {
    const int j = static_cast<int>(nextRandom(state) % static_cast<uint32_t>(i + 1));
    const uint8_t value = values[i];
    values[i] = values[j];
    values[j] = value;
  }
}

inline int clueTarget(const Difficulty difficulty, uint32_t& state) {
  int minimum = 0;
  int maximum = 0;
  switch (difficulty) {
    case Difficulty::Easy:
      minimum = 40;
      maximum = 45;
      break;
    case Difficulty::Normal:
      minimum = 35;
      maximum = 39;
      break;
    case Difficulty::Hard:
      minimum = 30;
      maximum = 34;
      break;
    case Difficulty::Expert:
      minimum = 26;
      maximum = 29;
      break;
    case Difficulty::Dev:
      return 80;  // Temporary dev/test difficulty: one empty cell.
  }
  return minimum + static_cast<int>(nextRandom(state) % static_cast<uint32_t>(maximum - minimum + 1));
}

inline uint16_t candidates(const Board& board, const int cell) {
  const int row = cell / SIDE;
  const int column = cell % SIDE;
  uint16_t used = 0;
  for (int i = 0; i < SIDE; ++i) {
    used |= static_cast<uint16_t>(1u << board[row * SIDE + i]);
    used |= static_cast<uint16_t>(1u << board[i * SIDE + column]);
    const int boxCell = ((row / 3) * 3 + i / 3) * SIDE + (column / 3) * 3 + i % 3;
    used |= static_cast<uint16_t>(1u << board[boxCell]);
  }
  return static_cast<uint16_t>(ALL_DIGITS & ~used);
}

inline void countSolutions(Board& board, int& count, const int limit) {
  if (count >= limit) return;

  int bestCell = -1;
  uint16_t bestCandidates = 0;
  int bestCount = SIDE + 1;
  for (int cell = 0; cell < CELL_COUNT; ++cell) {
    if (board[cell] != 0) continue;
    const uint16_t choices = candidates(board, cell);
    int choiceCount = 0;
    for (uint16_t bits = choices; bits != 0; bits &= static_cast<uint16_t>(bits - 1)) ++choiceCount;
    if (choiceCount == 0) return;
    if (choiceCount < bestCount) {
      bestCell = cell;
      bestCandidates = choices;
      bestCount = choiceCount;
      if (bestCount == 1) break;
    }
  }

  if (bestCell < 0) {
    ++count;
    return;
  }

  for (uint8_t digit = 1; digit <= SIDE && count < limit; ++digit) {
    if ((bestCandidates & (1u << digit)) == 0) continue;
    board[bestCell] = digit;
    countSolutions(board, count, limit);
    board[bestCell] = 0;
  }
}

inline int solutionCount(const Board& board, const int limit = 2) {
  Board working = board;
  int count = 0;
  countSolutions(working, count, limit);
  return count;
}

inline void generate(const Difficulty difficulty, uint32_t seed, Puzzle& puzzle) {
  if (seed == 0) seed = 0x9E3779B9u;
  uint8_t bands[] = {0, 1, 2};
  uint8_t stacks[] = {0, 1, 2};
  uint8_t rows[] = {0, 1, 2};
  uint8_t columns[] = {0, 1, 2};
  uint8_t digits[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t sourceRows[SIDE];
  uint8_t sourceColumns[SIDE];
  shuffle(bands, 3, seed);
  shuffle(stacks, 3, seed);
  shuffle(digits, SIDE, seed);
  for (int group = 0; group < 3; ++group) {
    shuffle(rows, 3, seed);
    shuffle(columns, 3, seed);
    for (int i = 0; i < 3; ++i) {
      sourceRows[group * 3 + i] = static_cast<uint8_t>(bands[group] * 3 + rows[i]);
      sourceColumns[group * 3 + i] = static_cast<uint8_t>(stacks[group] * 3 + columns[i]);
    }
  }

  Board generated{};
  for (int row = 0; row < SIDE; ++row) {
    const int sourceRow = sourceRows[row];
    for (int column = 0; column < SIDE; ++column) {
      const int sourceColumn = sourceColumns[column];
      const int pattern = (sourceRow * 3 + sourceRow / 3 + sourceColumn) % SIDE;
      generated[row * SIDE + column] = digits[pattern];
    }
  }
  puzzle.solution = generated;
  puzzle.givens = generated;

  uint8_t removalOrder[CELL_COUNT];
  for (int i = 0; i < CELL_COUNT; ++i) removalOrder[i] = static_cast<uint8_t>(i);
  shuffle(removalOrder, CELL_COUNT, seed);
  const int target = clueTarget(difficulty, seed);
  int clues = CELL_COUNT;
  for (const uint8_t cell : removalOrder) {
    if (clues <= target) break;
    const uint8_t saved = puzzle.givens[cell];
    puzzle.givens[cell] = 0;
    if (solutionCount(puzzle.givens) == 1) {
      --clues;
    } else {
      puzzle.givens[cell] = saved;
    }
  }
}

inline bool toggleNote(Notes& notes, const int cell, const int digit) {
  if (cell < 0 || cell >= CELL_COUNT || digit < 1 || digit > SIDE) return false;
  notes[cell] ^= static_cast<uint16_t>(1u << digit);
  return true;
}

inline int digitForPaletteSlot(const int slot) {
  return slot >= 0 && slot < SIDE ? slot + 1 : -1;
}

inline uint8_t cyclePickerSelection(const uint8_t selection, const int direction) {
  if (direction < 0) return selection == 0 ? SIDE : (selection == 1 ? 0 : selection - 1);
  return selection == 0 ? 1 : (selection == SIDE ? 0 : selection + 1);
}

inline bool clearConflictingNotes(Notes& notes, const int cell, const int digit) {
  if (cell < 0 || cell >= CELL_COUNT || digit < 1 || digit > SIDE) return false;
  const int row = cell / SIDE;
  const int column = cell % SIDE;
  const uint16_t digitMask = static_cast<uint16_t>(~(1u << digit));
  for (int i = 0; i < SIDE; ++i) {
    notes[row * SIDE + i] &= digitMask;
    notes[i * SIDE + column] &= digitMask;
    const int boxCell = ((row / 3) * 3 + i / 3) * SIDE + (column / 3) * 3 + i % 3;
    notes[boxCell] &= digitMask;
  }
  return true;
}

inline bool isComplete(const Board& board, const Board& solution) {
  for (int i = 0; i < CELL_COUNT; ++i) {
    if (board[i] != solution[i]) return false;
  }
  return true;
}

inline bool isValidSolution(const Board& board) {
  for (int row = 0; row < SIDE; ++row) {
    uint16_t seen = 0;
    for (int column = 0; column < SIDE; ++column) {
      const uint8_t digit = board[row * SIDE + column];
      if (digit < 1 || digit > SIDE || (seen & (1u << digit)) != 0) return false;
      seen |= static_cast<uint16_t>(1u << digit);
    }
  }
  for (int column = 0; column < SIDE; ++column) {
    uint16_t seen = 0;
    for (int row = 0; row < SIDE; ++row) {
      const uint8_t digit = board[row * SIDE + column];
      if ((seen & (1u << digit)) != 0) return false;
      seen |= static_cast<uint16_t>(1u << digit);
    }
  }
  for (int boxRow = 0; boxRow < 3; ++boxRow) {
    for (int boxColumn = 0; boxColumn < 3; ++boxColumn) {
      uint16_t seen = 0;
      for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
          const uint8_t digit = board[(boxRow * 3 + row) * SIDE + boxColumn * 3 + column];
          if ((seen & (1u << digit)) != 0) return false;
          seen |= static_cast<uint16_t>(1u << digit);
        }
      }
    }
  }
  return true;
}

}  // namespace sudoku
