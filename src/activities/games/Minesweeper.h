#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>

namespace minesweeper {

constexpr int MAX_W = 14;
constexpr int MAX_H = 14;
constexpr int MAX_CELLS = MAX_W * MAX_H;

enum class Difficulty : uint8_t { Easy = 0, Normal = 1, Hard = 2, Expert = 3 };

struct Config {
  int w;
  int h;
  int mines;
};

inline Config configFor(Difficulty d) {
  switch (d) {
    case Difficulty::Easy: return {8, 8, 10};
    case Difficulty::Normal: return {10, 10, 18};
    case Difficulty::Hard: return {12, 12, 28};
    case Difficulty::Expert: return {14, 14, 36};
  }
  return {8, 8, 10};
}

struct Board {
  Config cfg{8, 8, 10};
  std::array<uint8_t, MAX_CELLS> mine{};     // 0/1
  std::array<uint8_t, MAX_CELLS> revealed{};  // 0/1
  std::array<uint8_t, MAX_CELLS> flagged{};   // 0/1
  std::array<uint8_t, MAX_CELLS> adjacent{};  // 0-8
  bool generated = false;
  bool exploded = false;
  bool won = false;
  int revealedCount = 0;

  int idx(int x, int y) const { return y * cfg.w + x; }
  bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < cfg.w && y < cfg.h; }
};

inline void computeAdjacency(Board& b) {
  for (int y = 0; y < b.cfg.h; ++y) {
    for (int x = 0; x < b.cfg.w; ++x) {
      const int i = b.idx(x, y);
      if (b.mine[i]) { b.adjacent[i] = 9; continue; }
      uint8_t count = 0;
      for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        const int nx = x + dx, ny = y + dy;
        if (b.inBounds(nx, ny) && b.mine[b.idx(nx, ny)]) ++count;
      }
      b.adjacent[i] = count;
    }
  }
}

inline void generate(Board& b, uint32_t& rng, int safeX, int safeY) {
  b.mine.fill(0);
  b.revealed.fill(0);
  b.flagged.fill(0);
  b.adjacent.fill(0);
  b.generated = true;
  b.exploded = false;
  b.won = false;
  b.revealedCount = 0;

  const int total = b.cfg.w * b.cfg.h;
  int placed = 0;
  auto nextRand = [&rng]() -> uint32_t { rng = rng * 1664525u + 1013904223u; return rng; };
  while (placed < b.cfg.mines && placed < total - 1) {
    const int p = static_cast<int>(nextRand() % static_cast<uint32_t>(total));
    const int x = p % b.cfg.w;
    const int y = p / b.cfg.w;
    if (b.mine[p]) continue;
    // keep first click and its neighbors safe for playability
    if (std::abs(x - safeX) <= 1 && std::abs(y - safeY) <= 1) continue;
    b.mine[p] = 1;
    ++placed;
  }
  // if board too constrained (e.g. tiny), allow any except safe cell
  if (placed < b.cfg.mines) {
    for (int p = 0; p < total && placed < b.cfg.mines; ++p) {
      const int x = p % b.cfg.w, y = p / b.cfg.w;
      if (x == safeX && y == safeY) continue;
      if (!b.mine[p]) { b.mine[p] = 1; ++placed; }
    }
  }
  computeAdjacency(b);
}

inline void checkWin(Board& b) {
  const int total = b.cfg.w * b.cfg.h;
  if (!b.exploded && b.revealedCount == total - b.cfg.mines) b.won = true;
}

inline void reveal(Board& b, uint32_t& rng, int x, int y) {
  if (!b.inBounds(x, y) || b.exploded || b.won) return;
  const int i = b.idx(x, y);
  if (b.flagged[i] || b.revealed[i]) return;
  if (!b.generated) generate(b, rng, x, y);
  if (b.mine[i]) {
    b.revealed[i] = 1;
    b.exploded = true;
    // reveal all mines
    for (int j = 0; j < b.cfg.w * b.cfg.h; ++j) if (b.mine[j]) b.revealed[j] = 1;
    return;
  }

  // flood fill for zeros using explicit stack (bounded)
  struct Cell { int8_t x, y; };
  std::array<Cell, MAX_CELLS> stack{};
  int sp = 0;
  stack[sp++] = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
  while (sp > 0) {
    const Cell c = stack[--sp];
    if (!b.inBounds(c.x, c.y)) continue;
    const int ci = b.idx(c.x, c.y);
    if (b.revealed[ci] || b.flagged[ci] || b.mine[ci]) continue;
    b.revealed[ci] = 1;
    ++b.revealedCount;
    if (b.adjacent[ci] == 0) {
      for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        const int nx = c.x + dx, ny = c.y + dy;
        if (b.inBounds(nx, ny) && !b.revealed[b.idx(nx, ny)] && sp < MAX_CELLS) {
          stack[sp++] = {static_cast<int8_t>(nx), static_cast<int8_t>(ny)};
        }
      }
    }
  }
  checkWin(b);
}

inline void toggleFlag(Board& b, int x, int y) {
  if (!b.inBounds(x, y) || b.exploded || b.won) return;
  const int i = b.idx(x, y);
  if (b.revealed[i]) return;
  b.flagged[i] = !b.flagged[i];
}

inline int flaggedCount(const Board& b) {
  int c = 0;
  for (int i = 0; i < b.cfg.w * b.cfg.h; ++i) if (b.flagged[i]) ++c;
  return c;
}

}  // namespace minesweeper
