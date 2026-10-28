#pragma once

// Reader toolbar tools, in on-screen order. Contents, Text and More open a
// panel; the two bookmark tools act immediately.
enum ReaderTool : int {
  READER_TOOL_CONTENTS = 0,
  READER_TOOL_TEXT,
  READER_TOOL_BOOKMARK,
  READER_TOOL_LATEST_BOOKMARK,
  READER_TOOL_MORE,
  READER_TOOL_COUNT
};

inline bool readerToolOpensPanel(const int tool) {
  return tool == READER_TOOL_CONTENTS || tool == READER_TOOL_TEXT || tool == READER_TOOL_MORE;
}

inline int stepReaderTool(const int tool, const int delta) {
  return ((tool + delta) % READER_TOOL_COUNT + READER_TOOL_COUNT) % READER_TOOL_COUNT;
}
