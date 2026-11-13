#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

class ConnectFourProgressStore : public PersistableStore<ConnectFourProgressStore> {
  ConnectFourProgressStore() = default;

  friend class PersistableStore<ConnectFourProgressStore>;

 public:
  static constexpr int COLS = 7;
  static constexpr int ROWS = 6;

  enum class Cell : uint8_t { Empty = 0, P1 = 1, P2 = 2 };
  enum class State : uint8_t { Playing = 0, P1Win = 1, P2Win = 2, Draw = 3 };
  enum class Mode : uint8_t { VsAI = 0, PvP = 1 };
  enum class Difficulty : uint8_t { Easy = 0, Normal = 1, Hard = 2 };
  enum class Screen : uint8_t { ModeSelect = 0, DifficultySelect = 1, Playing = 2 };

  static const char* getFilePath() { return "/.crosspoint/connectfour.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool saveGame(Screen screen, const std::array<Cell, COLS * ROWS>& board, int cursorCol, State state, Mode mode,
                Difficulty difficulty, Cell current, uint32_t rngState);
  void clear();

  bool hasSavedGame() const { return savedGame; }
  Screen screen() const { return savedScreen; }
  const std::array<Cell, COLS * ROWS>& board() const { return savedBoard; }
  int cursorCol() const { return savedCursorCol; }
  State state() const { return savedState; }
  Mode mode() const { return savedMode; }
  Difficulty difficulty() const { return savedDifficulty; }
  Cell current() const { return savedCurrent; }
  uint32_t rngState() const { return savedRngState; }

 private:
  bool savedGame = false;
  Screen savedScreen = Screen::ModeSelect;
  std::array<Cell, COLS * ROWS> savedBoard{};
  int savedCursorCol = COLS / 2;
  State savedState = State::Playing;
  Mode savedMode = Mode::VsAI;
  Difficulty savedDifficulty = Difficulty::Normal;
  Cell savedCurrent = Cell::P1;
  uint32_t savedRngState = 1;
};
