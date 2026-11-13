#include "NotesActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
bool endsWithMd(const std::string& name) {
  return name.size() >= 3 && name.compare(name.size() - 3, 3, ".md") == 0;
}

std::string sanitizeNoteName(std::string name) {
  // trim spaces
  while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
  while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
  std::string out;
  out.reserve(name.size());
  for (char c : name) {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
        c == '\r' || c == '\n') {
      continue;
    }
    out.push_back(c);
  }
  if (out == "." || out == "..") out.clear();
  if (out.size() > 48) out.resize(48);
  return out;
}

std::string trimLine(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) b++;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) e--;
  return s.substr(b, e - b);
}
}  // namespace

NotesActivity::NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Notes", renderer, mappedInput) {}

void NotesActivity::onEnter() {
  Activity::onEnter();
  ensureNotesDir();
  scanNotes();
  requestUpdate();
}

bool NotesActivity::ensureNotesDir() const {
  if (!Storage.exists(kNotesDir)) {
    return Storage.mkdir(kNotesDir);
  }
  return true;
}

std::string NotesActivity::notePath(const std::string& name) const {
  return std::string(kNotesDir) + "/" + name + ".md";
}

void NotesActivity::scanNotes() {
  notes.clear();
  if (!ensureNotesDir()) return;
  auto dir = Storage.open(kNotesDir);
  if (!dir || !dir.isDirectory()) return;
  char nameBuf[128];
  for (auto entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    if (entry.isDirectory()) continue;
    nameBuf[0] = '\0';
    entry.getName(nameBuf, sizeof(nameBuf));
    std::string fname(nameBuf);
    if (!endsWithMd(fname)) continue;
    NoteEntry ne;
    ne.name = fname.substr(0, fname.size() - 3);
    // quick count
    std::string doc;
    {
      HalFile f;
      if (Storage.openFileForRead("NOTES", notePath(ne.name), f)) {
        const size_t sz = static_cast<size_t>(f.size());
        const size_t want = sz > 4096 ? 4096 : sz;
        doc.assign(want, '\0');
        if (want > 0) {
          const int rd = f.read(&doc[0], want);
          if (rd > 0) doc.resize(static_cast<size_t>(rd));
          else doc.clear();
        }
      }
    }
    // count tasks roughly
    size_t i = 0;
    while (i <= doc.size()) {
      size_t j = i;
      while (j < doc.size() && doc[j] != '\n') j++;
      std::string line = doc.substr(i, j - i);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      std::string t = trimLine(line);
      if (t.rfind("- [", 0) == 0 && t.size() >= 5 && t[4] == ']') {
        ne.total++;
        if (t[3] == 'x' || t[3] == 'X') ne.done++;
      }
      if (j >= doc.size()) break;
      i = j + 1;
    }
    notes.push_back(std::move(ne));
  }
  std::sort(notes.begin(), notes.end(), [](const NoteEntry& a, const NoteEntry& b) {
    return a.name < b.name;
  });
  if (listIndex >= static_cast<int>(notes.size())) listIndex = std::max(0, static_cast<int>(notes.size()) - 1);
}

void NotesActivity::rebuildLines() {
  lines.clear();
  size_t i = 0;
  while (i <= openDoc.size()) {
    size_t j = i;
    while (j < openDoc.size() && openDoc[j] != '\n') j++;
    std::string raw = openDoc.substr(i, j - i);
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    Line l;
    // parse task
    size_t p = 0;
    while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) p++;
    if (p + 5 <= raw.size() && raw[p] == '-' && raw[p + 1] == ' ' && raw[p + 2] == '[' &&
        (raw[p + 3] == ' ' || raw[p + 3] == 'x' || raw[p + 3] == 'X') && raw[p + 4] == ']') {
      l.isTask = true;
      l.checked = (raw[p + 3] == 'x' || raw[p + 3] == 'X');
      size_t t = p + 5;
      if (t < raw.size() && raw[t] == ' ') t++;
      l.text = raw.substr(t);
    } else {
      l.isTask = false;
      l.checked = false;
      l.text = raw;
    }
    lines.push_back(std::move(l));
    if (j >= openDoc.size()) break;
    i = j + 1;
  }
  if (lines.empty()) {
    lines.push_back(Line{});
  }
  if (noteIndex > static_cast<int>(lines.size())) noteIndex = static_cast<int>(lines.size()); // allow add slot
}

void NotesActivity::openNote(int index) {
  if (index < 0 || index >= static_cast<int>(notes.size())) return;
  openName = notes[index].name;
  loadOpenNote();
  view = View::Note;
  noteIndex = 0;
  requestUpdate();
}

void NotesActivity::loadOpenNote() {
  openDoc.clear();
  HalFile f;
  if (Storage.openFileForRead("NOTES", notePath(openName), f)) {
    const size_t sz = static_cast<size_t>(f.size());
    const size_t want = sz > 16384 ? 16384 : sz;
    openDoc.assign(want, '\0');
    if (want > 0) {
      const int rd = f.read(&openDoc[0], want);
      if (rd > 0) openDoc.resize(static_cast<size_t>(rd));
      else openDoc.clear();
    }
  }
  rebuildLines();
}

bool NotesActivity::saveOpenNote() {
  std::string out;
  out.reserve(openDoc.size() + lines.size() * 8);
  for (size_t i = 0; i < lines.size(); ++i) {
    const auto& l = lines[i];
    // skip trailing single empty line unless it's the only line? keep content as-is
    if (l.isTask) {
      out += "- [";
      out += l.checked ? 'x' : ' ';
      out += "] ";
      out += l.text;
    } else {
      out += l.text;
    }
    if (i + 1 < lines.size()) out += "\n";
  }
  openDoc = out;
  const std::string path = notePath(openName);
  HalFile f;
  if (!Storage.openFileForWrite("NOTES", path, f)) {
    LOG_ERR("NOTES", "Failed to open %s for write", path.c_str());
    return false;
  }
  if (!openDoc.empty() && f.write(openDoc.data(), openDoc.size()) != static_cast<int>(openDoc.size())) {
    LOG_ERR("NOTES", "Failed to write %s", path.c_str());
    return false;
  }
  return true;
}

int NotesActivity::listRowHeight() const { return std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 18); }
int NotesActivity::noteRowHeight() const { return std::max(40, renderer.getLineHeight(UI_10_FONT_ID) + 16); }
int NotesActivity::listTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + 6;
}


void NotesActivity::createNotePrompt() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "New note", "", 48);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) { requestUpdate(); return; }
    const auto& k = std::get<KeyboardResult>(result.data);
    const std::string name = sanitizeNoteName(k.text);
    if (name.empty()) { requestUpdate(); return; }
    if (Storage.exists(notePath(name).c_str())) { requestUpdate(); return; }
    openName = name;
    openDoc.clear();
    lines.clear();
    lines.push_back(Line{});
    view = View::Note;
    noteIndex = 0;
    saveOpenNote();
    scanNotes();
    requestUpdate();
  });
}

void NotesActivity::renameNotePrompt() {
  if (openName.empty()) return;
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Rename note", openName, 48);
  if (!keyboard) return;
  const std::string oldName = openName;
  startActivityForResult(std::move(keyboard), [this, oldName](const ActivityResult& result) {
    if (result.isCancelled) { requestUpdate(); return; }
    const auto& k = std::get<KeyboardResult>(result.data);
    const std::string name = sanitizeNoteName(k.text);
    if (name.empty() || name == oldName) { requestUpdate(); return; }
    if (Storage.exists(notePath(name).c_str())) { requestUpdate(); return; }
    Storage.rename(notePath(oldName).c_str(), notePath(name).c_str());
    openName = name;
    scanNotes();
    requestUpdate();
  });
}

void NotesActivity::addLinePrompt() {
  const int insertAt = std::clamp(noteIndex, 0, static_cast<int>(lines.size()));
  addLinePromptAt(insertAt);
}

void NotesActivity::addLinePromptAt(int insertAt) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Add line", "", 200);
  if (!keyboard) return;
  insertAt = std::clamp(insertAt, 0, static_cast<int>(lines.size()));
  startActivityForResult(std::move(keyboard), [this, insertAt](const ActivityResult& result) {
    if (result.isCancelled) { requestUpdate(); return; }
    const auto& k = std::get<KeyboardResult>(result.data);
    std::string text = k.text;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    Line l;
    l.isTask = false;
    l.checked = false;
    l.text = text;
    std::string t = trimLine(text);
    if (t.rfind("- [ ] ", 0) == 0 || t.rfind("- [x] ", 0) == 0 || t.rfind("- [X] ", 0) == 0) {
      l.isTask = true;
      l.checked = (t.size() >= 4 && (t[3] == 'x' || t[3] == 'X'));
      l.text = t.substr(6);
    } else if (t.rfind("[] ", 0) == 0) {
      l.isTask = true; l.checked = false; l.text = t.substr(3);
    } else if (t.rfind("[x] ", 0) == 0 || t.rfind("[X] ", 0) == 0) {
      l.isTask = true; l.checked = true; l.text = t.substr(4);
    }
    if (lines.size() == 1 && trimLine(lines[0].text).empty() && !lines[0].isTask) {
      lines[0] = l;
      noteIndex = 0;
    } else {
      const int at = std::clamp(insertAt, 0, static_cast<int>(lines.size()));
      lines.insert(lines.begin() + at, l);
      noteIndex = at;
    }
    saveOpenNote();
    requestUpdate();
  });
}

void NotesActivity::editLinePrompt(int idx) {
  if (idx < 0 || idx >= static_cast<int>(lines.size())) return;
  const std::string initial = lines[idx].text;
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Edit line", initial, 200);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this, idx](const ActivityResult& result) {
    if (result.isCancelled) { requestUpdate(); return; }
    if (idx < 0 || idx >= static_cast<int>(lines.size())) { requestUpdate(); return; }
    const auto& k = std::get<KeyboardResult>(result.data);
    std::string text = k.text;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    lines[idx].text = text;
    saveOpenNote();
    requestUpdate();
  });
}

void NotesActivity::deleteLine(int idx) {
  if (idx < 0 || idx >= static_cast<int>(lines.size())) return;
  lines.erase(lines.begin() + idx);
  if (lines.empty()) lines.push_back(Line{});
  if (noteIndex >= static_cast<int>(lines.size())) noteIndex = static_cast<int>(lines.size()) - 1;
  if (noteIndex < 0) noteIndex = 0;
  saveOpenNote();
  requestUpdate();
}

void NotesActivity::toggleLine(int idx) {
  if (idx < 0 || idx >= static_cast<int>(lines.size())) return;
  auto& l = lines[idx];
  if (!l.isTask) {
    if (!trimLine(l.text).empty()) l.isTask = true;
  }
  if (l.isTask) l.checked = !l.checked;
  saveOpenNote();
  requestUpdate();
}

void NotesActivity::duplicateNote(int idx) {
  if (idx < 0 || idx >= static_cast<int>(notes.size())) return;
  const std::string src = notes[idx].name;
  std::string base = src + " copy";
  std::string candidate = base;
  int n = 2;
  while (Storage.exists(notePath(candidate).c_str())) {
    char buf[64];
    snprintf(buf, sizeof(buf), " copy %d", n++);
    candidate = src + buf;
    if (candidate.size() > 48) candidate = candidate.substr(0, 48);
  }
  // copy file
  std::string doc;
  HalFile in;
  if (Storage.openFileForRead("NOTES", notePath(src), in)) {
    const size_t sz = static_cast<size_t>(in.size());
    const size_t want = sz > 16384 ? 16384 : sz;
    doc.assign(want, '\0');
    if (want > 0) {
      const int rd = in.read(&doc[0], want);
      if (rd > 0) doc.resize(static_cast<size_t>(rd)); else doc.clear();
    }
  }
  HalFile out;
  if (Storage.openFileForWrite("NOTES", notePath(candidate), out)) {
    if (!doc.empty()) out.write(doc.data(), doc.size());
  }
  scanNotes();
  requestUpdate();
}

void NotesActivity::deleteNoteAt(int idx) {
  if (idx < 0 || idx >= static_cast<int>(notes.size())) return;
  Storage.remove(notePath(notes[idx].name).c_str());
  scanNotes();
  requestUpdate();
}

void NotesActivity::showNoteOptions(int idx) {
  if (idx < 0 || idx >= static_cast<int>(notes.size())) return;
  listIndex = idx;
  const char* opts[] = {"Duplicate", "Remove", "Cancel"};
  optionPopup.show("Note", notes[idx].name.c_str(), opts, 3, 0, [this, idx](int choice) {
    if (choice == 0) duplicateNote(idx);
    else if (choice == 1) deleteNoteAt(idx);
    requestUpdate();
  });
  requestUpdate();
}

void NotesActivity::showLineOptions(int idx) {
  if (idx < 0 || idx >= static_cast<int>(lines.size())) return;
  noteIndex = idx;
  const char* opts[] = {"Add before", "Remove", "Add after", "Cancel"};
  optionPopup.show("Line", "Options", opts, 4, 0, [this, idx](int choice) {
    if (choice == 0) addLinePromptAt(idx);
    else if (choice == 1) deleteLine(idx);
    else if (choice == 2) addLinePromptAt(idx + 1);
    requestUpdate();
  });
  requestUpdate();
}

void NotesActivity::loop() {
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  if (view == View::List) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }
    const int count = static_cast<int>(notes.size());
    const int total = count + 1; // + New note
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { listIndex = (listIndex + total - 1) % total; requestUpdate(); return; }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { listIndex = (listIndex + 1) % total; requestUpdate(); return; }

    if (count > 0 && listIndex < count && mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, 700)) {
      showNoteOptions(listIndex);
      return;
    }
    int hx = 0, hy = 0;
    if (mappedInput.isScreenTouchHeld(hx, hy) && count > 0) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int top = listTop();
      const int rh = listRowHeight();
      const int margin = std::max(16, metrics.contentSidePadding);
      if (hx >= margin && hx < renderer.getScreenWidth() - margin && hy >= top) {
        const int idx = (hy - top) / rh;
        if (idx >= 0 && idx < count) {
          const uint32_t now = millis();
          if (touchHoldIndex != idx) { touchHoldIndex = idx; touchHoldStartMs = now; touchHoldFired = false; }
          else if (!touchHoldFired && now - touchHoldStartMs >= 700) {
            touchHoldFired = true;
            showNoteOptions(idx);
            return;
          }
        } else { touchHoldIndex = -1; }
      } else { touchHoldIndex = -1; }
    } else { touchHoldIndex = -1; touchHoldFired = false; }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (listIndex == count) createNotePrompt();
      else openNote(listIndex);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) && count > 0 && listIndex < count) {
      auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Rename note", notes[listIndex].name, 48);
      if (keyboard) {
        const std::string oldName = notes[listIndex].name;
        startActivityForResult(std::move(keyboard), [this, oldName](const ActivityResult& result) {
          if (result.isCancelled) { requestUpdate(); return; }
          const auto& k = std::get<KeyboardResult>(result.data);
          const std::string name = sanitizeNoteName(k.text);
          if (name.empty() || name == oldName) { requestUpdate(); return; }
          if (Storage.exists(notePath(name).c_str())) { requestUpdate(); return; }
          Storage.rename(notePath(oldName).c_str(), notePath(name).c_str());
          if (openName == oldName) openName = name;
          scanNotes();
          requestUpdate();
        });
      }
      return;
    }

    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      if (touchHoldFired) { touchHoldFired = false; return; }
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int top = listTop();
      const int rh = listRowHeight();
      const int margin = std::max(16, metrics.contentSidePadding);
      if (tx >= margin && tx < renderer.getScreenWidth() - margin && ty >= top) {
        const int idx = (ty - top) / rh;
        if (idx >= 0 && idx < total) {
          listIndex = idx;
          if (listIndex == count) createNotePrompt();
          else openNote(listIndex);
        }
      }
    }
    return;
  }

  // Note view
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveOpenNote();
    scanNotes();
    view = View::List;
    requestUpdate();
    return;
  }
  const int lineCount = static_cast<int>(lines.size());
  const int total = lineCount + 1; // + Add line
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { noteIndex = (noteIndex + total - 1) % total; requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { noteIndex = (noteIndex + 1) % total; requestUpdate(); return; }

  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, 700) && noteIndex < lineCount) {
    showLineOptions(noteIndex);
    return;
  }
  int shx = 0, shy = 0;
  if (mappedInput.isScreenTouchHeld(shx, shy)) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int top = listTop();
    const int rh = noteRowHeight();
    const int margin = std::max(16, metrics.contentSidePadding);
    if (shx >= margin && shx < renderer.getScreenWidth() - margin && shy >= top) {
      const int idx = (shy - top) / rh;
      if (idx >= 0 && idx < lineCount) {
        const uint32_t now = millis();
        if (touchHoldIndex != idx) { touchHoldIndex = idx; touchHoldStartMs = now; touchHoldFired = false; }
        else if (!touchHoldFired && now - touchHoldStartMs >= 700) {
          touchHoldFired = true;
          showLineOptions(idx);
          return;
        }
      } else { touchHoldIndex = -1; }
    } else { touchHoldIndex = -1; }
  } else { touchHoldIndex = -1; touchHoldFired = false; }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    renameNotePrompt();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    // delete whole note and return to list
    Storage.remove(notePath(openName).c_str());
    openName.clear();
    openDoc.clear();
    lines.clear();
    view = View::List;
    scanNotes();
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (noteIndex == lineCount) { addLinePrompt(); return; }
    auto& l = lines[noteIndex];
    if (l.isTask) { toggleLine(noteIndex); }
    else {
      if (trimLine(l.text).empty()) addLinePrompt();
      else editLinePrompt(noteIndex);
    }
    return;
  }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (touchHoldFired) { touchHoldFired = false; return; }
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int top = listTop();
    const int rh = noteRowHeight();
    const int margin = std::max(16, metrics.contentSidePadding);
    if (tx >= margin && tx < renderer.getScreenWidth() - margin && ty >= top) {
      const int idx = (ty - top) / rh;
      if (idx >= 0 && idx < total) {
        noteIndex = idx;
        if (noteIndex == lineCount) addLinePrompt();
        else if (lines[noteIndex].isTask) toggleLine(noteIndex);
        else if (trimLine(lines[noteIndex].text).empty()) addLinePrompt();
        else editLinePrompt(noteIndex);
      }
    }
  }
}

void NotesActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  renderer.clearScreen();
  if (view == View::List) renderList(); else renderNote();
  renderer.displayBuffer();
}

void NotesActivity::renderList() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Notes");
  const int top = listTop();
  const int rh = listRowHeight();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int count = static_cast<int>(notes.size());
  const int total = count + 1;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
  for (int i = 0; i < total; ++i) {
    const int y = top + i * rh;
    if (y + rh > bottom) break;
    const bool selected = i == listIndex;
    const int rowX = margin;
    const int rowW = renderer.getScreenWidth() - margin * 2;
    if (selected) renderer.fillRect(rowX, y, rowW, rh - 4);
    else renderer.drawRect(rowX, y, rowW, rh - 4);
    const int textY = y + (rh - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    char buf[96];
    if (i == count) {
      snprintf(buf, sizeof(buf), "+ New note");
    } else {
      if (notes[i].total > 0) snprintf(buf, sizeof(buf), "%s  (%d/%d)", notes[i].name.c_str(), notes[i].done, notes[i].total);
      else snprintf(buf, sizeof(buf), "%s", notes[i].name.c_str());
    }
    renderer.drawText(UI_12_FONT_ID, rowX + 12, textY, buf, !selected);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), "Rename");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void NotesActivity::renderNote() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, openName.c_str());
  const int top = listTop();
  const int rh = noteRowHeight();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int lineCount = static_cast<int>(lines.size());
  const int total = lineCount + 1;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
  for (int i = 0; i < total; ++i) {
    const int y = top + i * rh;
    if (y + rh > bottom) break;
    const bool selected = i == noteIndex;
    const int rowX = margin;
    const int rowW = renderer.getScreenWidth() - margin * 2;
    if (selected) renderer.fillRect(rowX, y, rowW, rh - 4);
    else renderer.drawRect(rowX, y, rowW, rh - 4);
    const int textY = y + (rh - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    char buf[160];
    if (i == lineCount) {
      snprintf(buf, sizeof(buf), "+ Add line");
    } else {
      const auto& l = lines[i];
      if (l.isTask) snprintf(buf, sizeof(buf), "[%c] %s", l.checked ? 'x' : ' ', l.text.c_str());
      else snprintf(buf, sizeof(buf), "%s", l.text.empty() ? "(empty line)" : l.text.c_str());
    }
    std::string s(buf);
    if (s.size() > 60) s = s.substr(0, 57) + "...";
    renderer.drawText(UI_10_FONT_ID, rowX + 10, textY, s.c_str(), !selected);
  }
  GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), "Rename", "Delete note");
}
