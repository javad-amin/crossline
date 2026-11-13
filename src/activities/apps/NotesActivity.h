#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class NotesActivity final : public Activity {
 public:
  NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { List, Note, Settings };
  enum class NotesTheme : uint8_t { Normal = 0, Notebook = 1 };
  enum class NotesFont : uint8_t { UI = 0, Small = 1, Handwriting = 2 };

  struct NoteEntry {
    std::string name;
    int total = 0;
    int done = 0;
  };

  struct Line {
    std::string text;
    bool isTask = false;
    bool checked = false;
  };

  static constexpr const char* kNotesDir = "/.crosspoint/notes";
  static constexpr const char* kNotesSettingsPath = "/.crosspoint/notes_app.json";

  bool ensureNotesDir() const;
  void scanNotes();
  void openNote(int index);
  void loadOpenNote();
  bool saveOpenNote();
  void rebuildLines();
  std::string notePath(const std::string& name) const;
  void loadSettings();
  void saveSettings() const;

  void renderList() const;
  void renderNote() const;
  void renderSettings() const;
  int listRowHeight() const;
  int noteRowHeight() const;
  int listTop() const;

  void createNotePrompt();
  void renameNotePrompt();
  void addLinePrompt();
  void addLinePromptAt(int insertAt);
  void editLinePrompt(int idx);
  void deleteLine(int idx);
  void toggleLine(int idx);
  void duplicateNote(int idx);
  void deleteNoteAt(int idx);
  void showNoteOptions(int idx);
  void showLineOptions(int idx);

  View view = View::List;
  NotesTheme notesTheme = NotesTheme::Notebook;
  NotesFont notesFont = NotesFont::Handwriting;
  int settingsIndex = 0; // 0 theme, 1 font
  std::vector<NoteEntry> notes;
  int listIndex = 0;

  std::string openName;
  std::string openDoc;
  std::vector<Line> lines;
  int noteIndex = 0;  // selected line, or lines.size() for "Add line"

  OptionPopup optionPopup;

  // Manual long-press fallback for options (list or note rows)
  uint32_t touchHoldStartMs = 0;
  int touchHoldIndex = -1;
  bool touchHoldFired = false;
};
