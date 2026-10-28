#include "BookReaderSettings.h"

#include <HalStorage.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

#include "CrossPointSettings.h"

namespace {
constexpr char BOOK_SETTINGS_DIR[] = "/.crosspoint/book_settings";

struct State {
  bool active = false;
  bool hadFile = false;
  char path[80] = "";
  ReaderTextSettings globals;
};
State state;

uint8_t readByte(JsonVariantConst doc, const char* key, const uint8_t fallback, const uint8_t min, const uint8_t max) {
  const uint8_t value = doc[key] | fallback;
  return value < min || value > max ? fallback : value;
}
}  // namespace

ReaderTextSettings ReaderTextSettings::capture(const CrossPointSettings& s) {
  ReaderTextSettings t;
  t.fontPointSize = s.fontPointSize;
  t.lineSpacing = s.lineSpacing;
  t.wordSpacing = s.wordSpacing;
  t.characterSpacing = s.characterSpacing;
  t.screenMargin = s.screenMargin;
  t.paragraphAlignment = s.paragraphAlignment;
  t.embeddedStyle = s.embeddedStyle;
  t.focusReadingEnabled = s.focusReadingEnabled;
  t.hyphenationEnabled = s.hyphenationEnabled;
  t.extraParagraphSpacing = s.extraParagraphSpacing;
  t.textAntiAliasing = s.textAntiAliasing;
  static_assert(sizeof(t.sdFontFamilyName) == sizeof(s.sdFontFamilyName));
  memcpy(t.sdFontFamilyName, s.sdFontFamilyName, sizeof(t.sdFontFamilyName));
  return t;
}

void ReaderTextSettings::applyTo(CrossPointSettings& s) const {
  s.fontPointSize = fontPointSize;
  s.lineSpacing = lineSpacing;
  s.wordSpacing = wordSpacing;
  s.characterSpacing = characterSpacing;
  s.screenMargin = screenMargin;
  s.paragraphAlignment = paragraphAlignment;
  s.embeddedStyle = embeddedStyle;
  s.focusReadingEnabled = focusReadingEnabled;
  s.hyphenationEnabled = hyphenationEnabled;
  s.extraParagraphSpacing = extraParagraphSpacing;
  s.textAntiAliasing = textAntiAliasing;
  memcpy(s.sdFontFamilyName, sdFontFamilyName, sizeof(s.sdFontFamilyName));
}

bool ReaderTextSettings::operator==(const ReaderTextSettings& o) const {
  return fontPointSize == o.fontPointSize && lineSpacing == o.lineSpacing && wordSpacing == o.wordSpacing &&
         characterSpacing == o.characterSpacing && screenMargin == o.screenMargin &&
         paragraphAlignment == o.paragraphAlignment && embeddedStyle == o.embeddedStyle &&
         focusReadingEnabled == o.focusReadingEnabled && hyphenationEnabled == o.hyphenationEnabled &&
         extraParagraphSpacing == o.extraParagraphSpacing && textAntiAliasing == o.textAntiAliasing &&
         strcmp(sdFontFamilyName, o.sdFontFamilyName) == 0;
}

namespace book_reader_settings {

void writeSettingsKeys(const ReaderTextSettings& v, JsonDocument& doc) {
  doc["fontSize"] = v.fontPointSize;
  doc["lineSpacing"] = v.lineSpacing;
  doc["wordSpacing"] = v.wordSpacing;
  doc["characterSpacing"] = v.characterSpacing;
  doc["screenMargin"] = v.screenMargin;
  doc["paragraphAlignment"] = v.paragraphAlignment;
  doc["embeddedStyle"] = v.embeddedStyle;
  doc["focusReadingEnabled"] = v.focusReadingEnabled;
  doc["hyphenationEnabled"] = v.hyphenationEnabled;
  doc["extraParagraphSpacing"] = v.extraParagraphSpacing;
  doc["textAntiAliasing"] = v.textAntiAliasing;
  if (v.sdFontFamilyName[0] != '\0') {
    doc["sdFontFamilyName"] = v.sdFontFamilyName;
  } else {
    doc.remove("sdFontFamilyName");
  }
}

void enter(const std::string& bookPath) {
  if (state.active) exit();
  state.globals = ReaderTextSettings::capture(SETTINGS);
  snprintf(state.path, sizeof(state.path), "%s/%zu.json", BOOK_SETTINGS_DIR, std::hash<std::string>{}(bookPath));
  state.active = true;

  JsonDocument doc;
  state.hadFile = PersistableStoreBase::readDocFromFile(state.path, doc);
  if (!state.hadFile) return;

  // Missing or out-of-range keys fall back to the device-wide value.
  const ReaderTextSettings& g = state.globals;
  ReaderTextSettings book = g;
  book.fontPointSize = readByte(doc, "fontSize", g.fontPointSize, 1, 255);
  book.lineSpacing = readByte(doc, "lineSpacing", g.lineSpacing, 0, CrossPointSettings::LINE_COMPRESSION_COUNT - 1);
  book.wordSpacing = readByte(doc, "wordSpacing", g.wordSpacing, CrossPointSettings::WORD_SPACING_MIN,
                              CrossPointSettings::WORD_SPACING_MAX);
  book.characterSpacing =
      readByte(doc, "characterSpacing", g.characterSpacing, 0, 2 * CrossPointSettings::CHARACTER_SPACING_OFFSET);
  book.screenMargin = readByte(doc, "screenMargin", g.screenMargin, CrossPointSettings::SCREEN_MARGIN_MIN,
                               CrossPointSettings::SCREEN_MARGIN_MAX);
  book.paragraphAlignment = readByte(doc, "paragraphAlignment", g.paragraphAlignment, 0,
                                     CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT - 1);
  book.embeddedStyle = readByte(doc, "embeddedStyle", g.embeddedStyle, 0, 1);
  book.focusReadingEnabled = readByte(doc, "focusReadingEnabled", g.focusReadingEnabled, 0, 1);
  book.hyphenationEnabled = readByte(doc, "hyphenationEnabled", g.hyphenationEnabled, 0, 1);
  book.extraParagraphSpacing = readByte(doc, "extraParagraphSpacing", g.extraParagraphSpacing, 0, 1);
  book.textAntiAliasing = readByte(doc, "textAntiAliasing", g.textAntiAliasing, 0, 1);
  const char* family = doc["sdFontFamilyName"] | "";
  strncpy(book.sdFontFamilyName, family, sizeof(book.sdFontFamilyName) - 1);
  book.sdFontFamilyName[sizeof(book.sdFontFamilyName) - 1] = '\0';
  book.applyTo(SETTINGS);
  LOG_DBG("BKS", "Applied per-book reader settings from %s", state.path);
}

void save() {
  if (!state.active) return;
  const ReaderTextSettings current = ReaderTextSettings::capture(SETTINGS);
  // Untouched books keep following the device-wide settings.
  if (!state.hadFile && current == state.globals) return;
  if (!Storage.exists(BOOK_SETTINGS_DIR) && !Storage.mkdir(BOOK_SETTINGS_DIR)) {
    LOG_ERR("BKS", "Cannot create %s", BOOK_SETTINGS_DIR);
    return;
  }
  JsonDocument doc;
  writeSettingsKeys(current, doc);
  if (PersistableStoreBase::writeDocToFile(state.path, doc)) state.hadFile = true;
}

void exit() {
  if (!state.active) return;
  save();
  state.globals.applyTo(SETTINGS);
  state.active = false;
}

const ReaderTextSettings* globalsWhileReading() { return state.active ? &state.globals : nullptr; }

}  // namespace book_reader_settings
