#pragma once

#include <ArduinoJson.h>

#include <cstdint>
#include <string>

class CrossPointSettings;

// The Text Settings fields, kept per book. While a book is open, SETTINGS holds
// that book's values; the device-wide values are parked here and are what
// settings.json keeps, so a book's choices never leak into other books.
struct ReaderTextSettings {
  uint8_t fontPointSize = 0;
  uint8_t lineSpacing = 0;
  uint8_t wordSpacing = 0;
  uint8_t characterSpacing = 0;
  uint8_t screenMargin = 0;
  uint8_t paragraphAlignment = 0;
  uint8_t embeddedStyle = 0;
  uint8_t focusReadingEnabled = 0;
  uint8_t hyphenationEnabled = 0;
  uint8_t extraParagraphSpacing = 0;
  uint8_t textAntiAliasing = 0;
  char sdFontFamilyName[32] = "";

  static ReaderTextSettings capture(const CrossPointSettings& settings);
  void applyTo(CrossPointSettings& settings) const;
  bool operator==(const ReaderTextSettings& other) const;
  bool operator!=(const ReaderTextSettings& other) const { return !(*this == other); }
};

namespace book_reader_settings {
// Parks the device-wide values and applies the book's saved ones, if any.
void enter(const std::string& bookPath);
// Saves the book's values once they differ from the device-wide ones.
void save();
// Saves, then restores the device-wide values.
void exit();
// Device-wide values while a book is open; nullptr otherwise.
const ReaderTextSettings* globalsWhileReading();
// Overwrites the Text Settings keys in a settings.json document.
void writeSettingsKeys(const ReaderTextSettings& values, JsonDocument& doc);
}  // namespace book_reader_settings
