#include "SudokuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "SudokuProgressStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long CONFIRM_HOLD_MS = 650;
constexpr int PENCIL_BUTTON_HEIGHT = 32;
constexpr int CONTROL_BUTTON_GAP = 8;
constexpr int NOTE_INSET = 2;
constexpr uint8_t NOTE_GLYPHS_5X7[9][7] = {
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
    {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},
    {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110},
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},
    {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110},
    {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110},
};
constexpr uint8_t NOTE_GLYPHS_3X5[9][5] = {
    {0b010, 0b110, 0b010, 0b010, 0b111}, {0b110, 0b001, 0b010, 0b100, 0b111}, {0b110, 0b001, 0b010, 0b001, 0b110},
    {0b101, 0b101, 0b111, 0b001, 0b001}, {0b111, 0b100, 0b110, 0b001, 0b110}, {0b011, 0b100, 0b110, 0b101, 0b010},
    {0b111, 0b001, 0b010, 0b010, 0b010}, {0b010, 0b101, 0b010, 0b101, 0b010}, {0b010, 0b101, 0b011, 0b001, 0b110},
};

const char* difficultyName(const sudoku::Difficulty difficulty) {
  switch (difficulty) {
    case sudoku::Difficulty::Easy:
      return tr(STR_SUDOKU_EASY);
    case sudoku::Difficulty::Normal:
      return tr(STR_SUDOKU_NORMAL);
    case sudoku::Difficulty::Hard:
      return tr(STR_SUDOKU_HARD);
    case sudoku::Difficulty::Expert:
      return tr(STR_SUDOKU_EXPERT);
    case sudoku::Difficulty::Dev:
      return "Dev (1 empty)";  // Temporary dev/test difficulty; hide once done.
  }
  return tr(STR_SUDOKU_EASY);
}
}  // namespace

SudokuActivity::SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Sudoku", renderer, mappedInput) {}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  auto& progress = SudokuProgressStore::getInstance();
  if (progress.loadFromFile() && progress.hasSavedGame()) {
    hasSavedGame = true;
    puzzle = progress.puzzle();
    entries = progress.entries();
    notes = progress.notes();
    difficulty = progress.difficulty();
    pencilMode = progress.pencilMode();
    screen = Screen::StartMenu;
  }
  requestUpdate();
}

void SudokuActivity::startPuzzle() {
  const uint32_t seed = static_cast<uint32_t>(random(2147483647L));
  sudoku::generate(difficulty, seed, puzzle);
  entries = puzzle.givens;
  notes.fill(0);
  selectedRow = 0;
  selectedColumn = 0;
  selectedDigit = 1;
  pencilMode = false;
  screen = Screen::Board;
  hasSavedGame = true;
  saveProgress();
  requestUpdate();
}

void SudokuActivity::continueGame() {
  screen = sudoku::isComplete(entries, puzzle.solution) ? Screen::Complete : Screen::Board;
  requestUpdate();
}

void SudokuActivity::saveProgress() {
  SudokuProgressStore::getInstance().saveGame(puzzle, entries, notes, difficulty, pencilMode);
}

void SudokuActivity::chooseNumber(const uint8_t digit) {
  const int cell = selectedRow * sudoku::SIDE + selectedColumn;
  if (puzzle.givens[cell] != 0) return;

  const bool wasNumberPicker = screen == Screen::NumberPicker;

  if (pencilMode && digit != 0) {
    sudoku::toggleNote(notes, cell, digit);
  } else {
    entries[cell] = digit;
    notes[cell] = 0;
    if (digit != 0) sudoku::clearConflictingNotes(notes, cell, digit);
    if (sudoku::isComplete(entries, puzzle.solution)) {
      screen = Screen::Complete;
    }
  }

  if (wasNumberPicker && screen != Screen::Complete) screen = Screen::Board;
  saveProgress();
  requestUpdate();
}

void SudokuActivity::moveSelection(const int rowDelta, const int columnDelta) {
  selectedRow = std::clamp(selectedRow + rowDelta, 0, sudoku::SIDE - 1);
  selectedColumn = std::clamp(selectedColumn + columnDelta, 0, sudoku::SIDE - 1);
  requestUpdate();
}

void SudokuActivity::loop() {
  if (screen == Screen::StartMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      startSelection = 1 - startSelection;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (startSelection == 0)
        continueGame();
      else {
        screen = Screen::Difficulty;
        requestUpdate();
      }
      return;
    }
    int x = 0;
    int y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
      const int index = (y - startMenuTop()) / rowHeight;
      const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
      if (x >= margin && x < renderer.getScreenWidth() - margin && index >= 0 && index < 2 && y >= startMenuTop()) {
        startSelection = index;
        if (startSelection == 0)
          continueGame();
        else {
          screen = Screen::Difficulty;
          requestUpdate();
        }
      }
    }
    return;
  }

  if (screen == Screen::Difficulty) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (hasSavedGame) {
        screen = Screen::StartMenu;
        requestUpdate();
      } else {
        finish();
      }
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      difficulty = static_cast<sudoku::Difficulty>((static_cast<int>(difficulty) + 3) % 4);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      difficulty = static_cast<sudoku::Difficulty>((static_cast<int>(difficulty) + 1) % 4);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startPuzzle();
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      const int rowHeight = std::max(42, renderer.getLineHeight(UI_12_FONT_ID) + 20);
      const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
      const int index = (y - difficultyListTop()) / rowHeight;
      if (x >= margin && x < renderer.getScreenWidth() - margin && index >= 0 && index < 4 &&
          y >= difficultyListTop()) {
        difficulty = static_cast<sudoku::Difficulty>(index);
        startPuzzle();
      }
    }
    return;
  }

  if (screen == Screen::Complete) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startPuzzle();
    } else {
      int x = 0, y = 0;
      if (mappedInput.wasScreenTapped(x, y)) startPuzzle();
    }
    return;
  }

  if (screen == Screen::NumberPicker) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      screen = Screen::Board;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      selectedDigit = sudoku::cyclePickerSelection(selectedDigit, -1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      selectedDigit = sudoku::cyclePickerSelection(selectedDigit, 1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      chooseNumber(selectedDigit);
      return;
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      const int digit = digitAt(x, y);
      if (digit >= 0) chooseNumber(static_cast<uint8_t>(digit));
    }
    return;
  }

  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, CONFIRM_HOLD_MS)) {
    pencilMode = !pencilMode;
    saveProgress();
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    moveSelection(-1, 0);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    moveSelection(1, 0);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveSelection(0, -1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    moveSelection(0, 1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    screen = Screen::NumberPicker;
    requestUpdate();
    return;
  }

  int x = 0, y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const int left = gridLeft();
  const int top = gridTop();
  const int size = gridSize();
  if (x >= left && x < left + size && y >= top && y < top + size) {
    selectedColumn = std::clamp((x - left) * sudoku::SIDE / size, 0, sudoku::SIDE - 1);
    selectedRow = std::clamp((y - top) * sudoku::SIDE / size, 0, sudoku::SIDE - 1);
    requestUpdate();
    return;
  }
  const int digit = digitAt(x, y);
  if (digit >= 0) {
    chooseNumber(static_cast<uint8_t>(digit));
    return;
  }
  const int controlsTop = controlButtonTop();
  const int clearWidth = clearButtonWidth();
  const int clearX = clearButtonLeft();
  if (x >= clearX && x < clearX + clearWidth && y >= controlsTop && y < controlsTop + PENCIL_BUTTON_HEIGHT) {
    chooseNumber(0);
    return;
  }
  const int pencilY = controlsTop;
  const int pencilWidth = pencilButtonWidth();
  const int pencilX = pencilButtonLeft();
  if (x >= pencilX && x < pencilX + pencilWidth && y >= pencilY && y < pencilY + PENCIL_BUTTON_HEIGHT) {
    pencilMode = !pencilMode;
    requestUpdate();
  }
}

int SudokuActivity::gridLeft() const { return (renderer.getScreenWidth() - gridSize()) / 2; }

int SudokuActivity::gridTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + renderer.getLineHeight(UI_10_FONT_ID) * 2 + 12;
}

int SudokuActivity::gridSize() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int availableWidth = renderer.getScreenWidth() - 24;
  const int bottomReserve = metrics.buttonHintsHeight + 160;
  const int availableHeight = renderer.getScreenHeight() - gridTop() - bottomReserve;
  const int maximum = std::max(9, std::min(availableWidth, availableHeight));
  return maximum - maximum % sudoku::SIDE;
}

int SudokuActivity::difficultyListTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(42, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int listHeight = rowHeight * 4;
  return metrics.topPadding + metrics.headerHeight +
         (renderer.getScreenHeight() - metrics.topPadding - metrics.headerHeight - metrics.buttonHintsHeight -
          listHeight) /
             2;
}

int SudokuActivity::startMenuTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int listHeight = rowHeight * 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight;
  return contentTop + std::max(0, (contentBottom - contentTop - listHeight) / 2);
}

int SudokuActivity::paletteTop() const { return gridTop() + gridSize() + 8; }

int SudokuActivity::digitButtonSize() const { return std::min(52, (renderer.getScreenWidth() - 24) / sudoku::SIDE); }

int SudokuActivity::controlButtonTop() const { return paletteTop() + digitButtonSize() + 14; }

int SudokuActivity::pencilButtonLeft() const { return 12; }

int SudokuActivity::pencilButtonWidth() const {
  constexpr int HORIZONTAL_PADDING = 24;
  const int offLabelWidth = renderer.getTextWidth(UI_10_FONT_ID, tr(STR_SUDOKU_PENCIL_OFF));
  const int onLabelWidth = renderer.getTextWidth(UI_10_FONT_ID, tr(STR_SUDOKU_PENCIL_ON));
  const int maxWidth = (renderer.getScreenWidth() - 24 - CONTROL_BUTTON_GAP) / 2;
  return std::min(maxWidth, std::max(offLabelWidth, onLabelWidth) + HORIZONTAL_PADDING);
}

int SudokuActivity::clearButtonLeft() const { return renderer.getScreenWidth() - 12 - clearButtonWidth(); }

int SudokuActivity::clearButtonWidth() const {
  constexpr int HORIZONTAL_PADDING = 24;
  const int maxWidth = (renderer.getScreenWidth() - 24 - CONTROL_BUTTON_GAP) / 2;
  return std::min(maxWidth, renderer.getTextWidth(UI_10_FONT_ID, tr(STR_CLEAR_BUTTON)) + HORIZONTAL_PADDING);
}

int SudokuActivity::digitAt(const int x, const int y) const {
  const int size = digitButtonSize();
  const int width = size * sudoku::SIDE;
  const int left = (renderer.getScreenWidth() - width) / 2;
  const int top = paletteTop();
  if (x < left || x >= left + width || y < top || y >= top + size) return -1;
  return sudoku::digitForPaletteSlot((x - left) / size);
}

void SudokuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_SUDOKU));

  switch (screen) {
    case Screen::StartMenu:
      drawStartMenu();
      break;
    case Screen::Difficulty:
      drawDifficulty();
      break;
    case Screen::Board:
      drawBoard();
      break;
    case Screen::NumberPicker:
      drawBoard();
      drawNumberPicker();
      break;
    case Screen::Complete:
      drawComplete();
      break;
  }
  drawFooter();
  renderer.displayBuffer();
}

void SudokuActivity::drawStartMenu() {
  const int rowHeight = std::max(54, renderer.getLineHeight(UI_12_FONT_ID) + 28);
  const int top = startMenuTop();
  const char* labels[] = {tr(STR_SUDOKU_CONTINUE_GAME), tr(STR_SUDOKU_NEW_GAME)};
  const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
  for (int i = 0; i < 2; ++i) {
    const int y = top + i * rowHeight;
    const bool selected = startSelection == i;
    if (selected) {
      renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    } else {
      renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, labels[i],
                              !selected);
  }
}

void SudokuActivity::drawDifficulty() {
  const int rowHeight = std::max(42, renderer.getLineHeight(UI_12_FONT_ID) + 20);
  const int listHeight = rowHeight * 4;
  const int top = difficultyListTop();
  renderer.drawCenteredText(UI_12_FONT_ID, top - renderer.getLineHeight(UI_12_FONT_ID) - 12,
                            tr(STR_SUDOKU_SELECT_DIFFICULTY));
  const char* labels[] = {tr(STR_SUDOKU_EASY), tr(STR_SUDOKU_NORMAL), tr(STR_SUDOKU_HARD), tr(STR_SUDOKU_EXPERT)};
  const int margin = std::max(20, UITheme::getInstance().getMetrics().contentSidePadding);
  const int listTop = difficultyListTop();
  for (int i = 0; i < 4; ++i) {
    const int y = listTop + i * rowHeight;
    const bool selected = static_cast<int>(difficulty) == i;
    if (selected) {
      renderer.fillRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    } else {
      renderer.drawRect(margin, y, renderer.getScreenWidth() - margin * 2, rowHeight - 4);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, labels[i],
                              !selected);
  }
  renderer.drawCenteredText(UI_10_FONT_ID, top + listHeight + 10, tr(STR_SUDOKU_DIFFICULTY_HELP));
}

void SudokuActivity::drawBoard() {
  char progress[48];
  int filled = 0;
  for (const uint8_t digit : entries) {
    if (digit != 0) ++filled;
  }
  snprintf(progress, sizeof(progress), tr(STR_SUDOKU_PROGRESS), filled);
  renderer.drawCenteredText(UI_10_FONT_ID, gridTop() - renderer.getLineHeight(UI_10_FONT_ID) - 2, progress);
  renderer.drawCenteredText(UI_10_FONT_ID, gridTop() - renderer.getLineHeight(UI_10_FONT_ID) * 2 - 4,
                            pencilMode ? tr(STR_SUDOKU_PENCIL_ON) : difficultyName(difficulty));
  drawGrid();
  drawNumberPalette();
  if (screen == Screen::NumberPicker) return;
  const int buttonWidth = pencilButtonWidth();
  const int x = pencilButtonLeft();
  const int y = controlButtonTop();
  if (pencilMode) {
    renderer.fillRect(x, y, buttonWidth, PENCIL_BUTTON_HEIGHT);
  } else {
    renderer.drawRect(x, y, buttonWidth, PENCIL_BUTTON_HEIGHT, 2, true);
  }
  const char* pencilLabel = pencilMode ? tr(STR_SUDOKU_PENCIL_ON) : tr(STR_SUDOKU_PENCIL_OFF);
  const int labelY = y + (PENCIL_BUTTON_HEIGHT - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  const int pencilLabelWidth = renderer.getTextWidth(UI_10_FONT_ID, pencilLabel);
  renderer.drawText(UI_10_FONT_ID, x + (buttonWidth - pencilLabelWidth) / 2, labelY, pencilLabel, !pencilMode);
  const int clearWidth = clearButtonWidth();
  const int clearX = clearButtonLeft();
  renderer.drawRect(clearX, y, clearWidth, PENCIL_BUTTON_HEIGHT, 2, true);
  const char* clearLabel = tr(STR_CLEAR_BUTTON);
  const int clearLabelWidth = renderer.getTextWidth(UI_10_FONT_ID, clearLabel);
  renderer.drawText(UI_10_FONT_ID, clearX + (clearWidth - clearLabelWidth) / 2, labelY, clearLabel);
  renderer.drawCenteredText(UI_10_FONT_ID, y + PENCIL_BUTTON_HEIGHT + 4,
                            mappedInput.hasTouch() ? tr(STR_SUDOKU_TOUCH_HELP) : tr(STR_SUDOKU_BUTTON_HELP));
}

void SudokuActivity::drawGrid() {
  const int left = gridLeft();
  const int top = gridTop();
  const int size = gridSize();
  const int cellSize = size / sudoku::SIDE;
  for (int row = 0; row < sudoku::SIDE; ++row) {
    for (int column = 0; column < sudoku::SIDE; ++column) {
      const int cell = row * sudoku::SIDE + column;
      const int x = left + column * cellSize;
      const int y = top + row * cellSize;
      const bool selected = row == selectedRow && column == selectedColumn;
      const bool given = puzzle.givens[cell] != 0;
      if (selected) {
        renderer.fillRect(x, y, cellSize, cellSize);
      } else if (given) {
        renderer.fillRectDither(x, y, cellSize, cellSize, Color::LightGray);
      } else {
        renderer.fillRect(x, y, cellSize, cellSize, false);
      }

      const uint8_t digit = entries[cell];
      if (digit != 0) {
        char text[2] = {static_cast<char>('0' + digit), '\0'};
        const int font = (cellSize >= 40) ? UI_12_FONT_ID : UI_10_FONT_ID;
        const int textWidth = renderer.getTextWidth(font, text);
        const int textHeight = renderer.getLineHeight(font);
        renderer.drawText(font, x + (cellSize - textWidth) / 2, y + (cellSize - textHeight) / 2, text, !selected);
      } else {
        for (uint8_t note = 1; note <= sudoku::SIDE; ++note) {
          if ((notes[cell] & (1u << note)) == 0) continue;
          const int noteRow = (note - 1) / 3;
          const int noteColumn = (note - 1) % 3;
          const int slotSize = (cellSize - 2 * NOTE_INSET) / 3;
          const int slotX = x + NOTE_INSET + noteColumn * slotSize;
          const int slotY = y + NOTE_INSET + noteRow * slotSize;
          const bool useLargeGlyph = slotSize >= 7;
          const int glyphWidth = useLargeGlyph ? 5 : 3;
          const int glyphHeight = useLargeGlyph ? 7 : 5;
          const int noteX = slotX + (slotSize - glyphWidth) / 2;
          const int noteY = slotY + (slotSize - glyphHeight) / 2;
          for (int glyphRow = 0; glyphRow < glyphHeight; ++glyphRow) {
            const uint8_t bits =
                useLargeGlyph ? NOTE_GLYPHS_5X7[note - 1][glyphRow] : NOTE_GLYPHS_3X5[note - 1][glyphRow];
            for (int glyphColumn = 0; glyphColumn < glyphWidth; ++glyphColumn) {
              if ((bits & (1u << (glyphWidth - glyphColumn - 1))) != 0) {
                renderer.drawPixel(noteX + glyphColumn, noteY + glyphRow, !selected);
              }
            }
          }
        }
      }
      renderer.drawRect(x, y, cellSize, cellSize, true);
    }
  }

  for (int line = 1; line < sudoku::SIDE; ++line) {
    if (line % 3 == 0) continue;
    const int offset = line * cellSize;
    renderer.drawLine(left + offset, top, left + offset, top + cellSize * sudoku::SIDE, true);
    renderer.drawLine(left, top + offset, left + cellSize * sudoku::SIDE, top + offset, true);
  }

  const int gridPixels = cellSize * sudoku::SIDE;
  renderer.drawRect(left, top, gridPixels, gridPixels, 3, true);
  for (int line = 3; line < sudoku::SIDE; line += 3) {
    const int offset = line * cellSize;
    renderer.fillRect(left + offset - 1, top, 3, gridPixels);
    renderer.fillRect(left, top + offset - 1, gridPixels, 3);
  }
}

void SudokuActivity::drawNumberPalette() {
  const int size = digitButtonSize();
  const int width = size * sudoku::SIDE;
  const int left = (renderer.getScreenWidth() - width) / 2;
  const int top = paletteTop();
  for (int slot = 0; slot < sudoku::SIDE; ++slot) {
    const int digit = sudoku::digitForPaletteSlot(slot);
    const int x = left + slot * size;
    const bool selected = screen == Screen::NumberPicker && selectedDigit == digit;
    if (selected)
      renderer.fillRect(x, top, size, size);
    else
      renderer.drawRect(x, top, size, size);
    char text[2] = {static_cast<char>('0' + digit), '\0'};
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text);
    const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawText(UI_12_FONT_ID, x + (size - textWidth) / 2, top + (size - textHeight) / 2, text, !selected);
  }
}

void SudokuActivity::drawNumberPicker() {
  const int y = controlButtonTop();
  renderer.drawCenteredText(UI_10_FONT_ID, y + 6,
                            selectedDigit == 0 ? tr(STR_CLEAR_BUTTON) : tr(STR_SUDOKU_PICK_NUMBER));
}

void SudokuActivity::drawComplete() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int center = (renderer.getScreenHeight() + metrics.topPadding + metrics.headerHeight) / 2;
  renderer.drawCenteredText(UI_12_FONT_ID, center - 24, tr(STR_SUDOKU_COMPLETE));
  renderer.drawCenteredText(UI_10_FONT_ID, center + 4, difficultyName(difficulty));
  renderer.drawCenteredText(UI_10_FONT_ID, center + 28, tr(STR_SUDOKU_NEW_PUZZLE));
}

void SudokuActivity::drawFooter() {
  const char* firstHint = screen == Screen::NumberPicker ? tr(STR_CANCEL) : tr(STR_BACK);
  const char* confirmHint = screen == Screen::Complete ? tr(STR_SUDOKU_NEW_PUZZLE) : tr(STR_SELECT);
  const char* leftHint = screen == Screen::Difficulty || screen == Screen::StartMenu ? tr(STR_DIR_UP)
                         : screen == Screen::Complete                                ? ""
                                                                                     : tr(STR_DIR_LEFT);
  const char* rightHint = screen == Screen::Difficulty || screen == Screen::StartMenu ? tr(STR_DIR_DOWN)
                          : screen == Screen::Complete                                ? ""
                                                                                      : tr(STR_DIR_RIGHT);
  const auto labels = mappedInput.mapLabels(firstHint, confirmHint, leftHint, rightHint);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
