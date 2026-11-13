#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"

class ConnectFourActivity final : public Activity {
 public:
  ConnectFourActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int COLS = 7;
  static constexpr int ROWS = 6;

  enum class Cell : uint8_t { Empty, P1, P2 };  // P1=X (human in AI mode), P2=O (AI)
  enum class State : uint8_t { Playing, P1Win, P2Win, Draw };
  enum class Mode : uint8_t { VsAI, PvP };
  enum class Difficulty : uint8_t { Easy = 0, Normal = 1, Hard = 2 };
  enum class Screen : uint8_t { ModeSelect, DifficultySelect, Playing };

  void resetBoard();
  bool dropInColumn(int col, Cell who);
  bool dropInBoard(std::array<Cell, COLS * ROWS>& b, int col, Cell who) const;
  bool checkWin(const std::array<Cell, COLS * ROWS>& b, Cell who) const;
  bool checkWin(Cell who) const { return checkWin(board, who); }
  bool isFull(const std::array<Cell, COLS * ROWS>& b) const;
  bool isFull() const { return isFull(board); }
  int aiMove();
  int scoreBoard(const std::array<Cell, COLS * ROWS>& b, Cell who) const;
  int minimax(std::array<Cell, COLS * ROWS>& b, int depth, int alpha, int beta, bool maximizing) const;
  void drawModeSelect() const;
  void drawDifficultySelect() const;
  void drawBoard() const;

  std::array<Cell, COLS * ROWS> board{};
  int cursorCol = COLS / 2;
  State state = State::Playing;
  Mode mode = Mode::VsAI;
  Difficulty difficulty = Difficulty::Normal;
  Screen screen = Screen::ModeSelect;
  Cell current = Cell::P1;
  uint32_t rngState = 1;
  bool needsRedraw = true;
};
