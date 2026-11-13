#pragma once

#include "Sudoku.h"
#include "activities/Activity.h"

class SudokuActivity final : public Activity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { StartMenu, Difficulty, Board, NumberPicker, Complete };
  void startPuzzle();
  void continueGame();
  void saveProgress();
  void chooseNumber(uint8_t digit);
  void moveSelection(int rowDelta, int columnDelta);
  void drawStartMenu();
  void drawDifficulty();
  void drawBoard();
  void drawNumberPicker();
  void drawComplete();
  void drawGrid();
  void drawNumberPalette();
  int gridLeft() const;
  int gridTop() const;
  int gridSize() const;
  int startMenuTop() const;
  int difficultyListTop() const;
  int paletteTop() const;
  int digitButtonSize() const;
  int controlButtonTop() const;
  int pencilButtonLeft() const;
  int pencilButtonWidth() const;
  int clearButtonLeft() const;
  int clearButtonWidth() const;
  int digitAt(int x, int y) const;
  void drawFooter();

  Screen screen = Screen::Difficulty;
  sudoku::Difficulty difficulty = sudoku::Difficulty::Easy;
  sudoku::Puzzle puzzle{};
  sudoku::Board entries{};
  sudoku::Notes notes{};
  int selectedRow = 0;
  int selectedColumn = 0;
  uint8_t selectedDigit = 1;
  bool pencilMode = false;
  bool hasSavedGame = false;
  int startSelection = 0;
};
