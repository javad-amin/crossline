#pragma once

#include <cstdint>
#include <array>

#include "ChessCore.h"
#include "ChessEngine.h"
#include "activities/Activity.h"

class ChessActivity final : public Activity {
 public:
  ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { StartMenu, ModeSelect, DifficultySelect, SideSelect, Playing };
  enum class Mode : uint8_t { VsComputer, PassAndPlay, FaceToFace };
  enum class State : uint8_t { Playing, Checkmate, Stalemate, Draw };

  static constexpr char kSavePath[] = "/.crosspoint/chess.sav";
  static constexpr char kSettingsPath[] = "/.crosspoint/chess.cfg";
  static constexpr int kSaveVersion = 3;
  static constexpr int kMaxHistory = 128;

  // Game
  chess::Position position{};
  chess::MoveList legalMoves{};
  chess::SearchBuffers searchBuffers{};
  std::array<char[10], kMaxHistory> history{};
  std::array<chess::Move, kMaxHistory> historyMove{};
  std::array<chess::Undo, kMaxHistory> historyUndo{};
  std::array<uint64_t, 128> repKeys{};
  int historyCount = 0;
  int historyBase = 0;
  int undoableFrom = 0;
  int repCount = 0;
  int selectedSquare = -1;
  int cursorSquare = 12;
  State state = State::Playing;
  bool engineThinking = false;
  bool needsRedraw = true;

  // UI / settings
  Screen screen = Screen::StartMenu;
  Mode mode = Mode::VsComputer;
  uint8_t level = chess::kDefaultLevel;
  bool humanPlaysWhite = true;
  bool showHints = true;
  bool hasSavedGame = false;
  int menuSelection = 0;

  // Persistence
  void saveGame() const;
  bool loadGame();
  void saveSettings() const;
  void loadSettings();

  // Logic
  void resetGame();
  void refreshLegalMoves();
  void resetRepetition();
  void pushRepetitionKey();
  bool vsComputer() const { return mode == Mode::VsComputer; }
  bool engineToMove() const;
  bool whiteAtBottom() const;
  void recordMove(const chess::Move& move);
  void applyMove(const chess::Move& move);
  void playEngineMove();
  bool canTakeBack() const;
  void takeBack();
  void handleSquareActivated(int sq);
  bool tryMove(int from, int to);
  const char* statusText() const;

  // UI helpers
  int startMenuTop() const;
  int modeMenuTop() const;
  int difficultyMenuTop() const;
  int sideMenuTop() const;
  void drawStartMenu() const;
  void drawModeSelect() const;
  void drawDifficultySelect() const;
  void drawSideSelect() const;
  void drawBoard() const;
  void drawPieceAtSquare(int square, int x, int y, int size) const;
  int squareAtPoint(int px, int py) const;
  void handleStartMenuInput();
  void handleModeSelectInput();
  void handleDifficultySelectInput();
  void handleSideSelectInput();
  void handlePlayingInput();
  void moveCursorVisual(int dx, int dy);
};
