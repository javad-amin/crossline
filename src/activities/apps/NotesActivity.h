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
  enum class View : uint8_t { List, Note };

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

  bool ensureNotesDir() const;
  void scanNotes();
  void openNote(int index);
  void loadOpenNote();
  bool saveOpenNote();
  void rebuildLines();
  std::string notePath(const std::string& name) const;

  void renderList() const;
  void renderNote() const;
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
