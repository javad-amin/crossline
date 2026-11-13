#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Identifies the sleep image a saved Home background was made from, so an
// unchanged image doesn't need decoding again on every sleep.
constexpr size_t HOME_BACKGROUND_KEY_LEN = 288;

inline bool makeHomeBackgroundKey(char* out, const size_t outLen, const char* imagePath, const uint32_t fileSize,
                                  const uint32_t modifiedTime, const bool inverted) {
  if (outLen == 0) return false;
  out[0] = '\0';
  if (!imagePath || !imagePath[0]) return false;
  const int written = snprintf(out, outLen, "%s|%lu|%lu|%d", imagePath, static_cast<unsigned long>(fileSize),
                               static_cast<unsigned long>(modifiedTime), inverted);
  if (written < 0 || static_cast<size_t>(written) >= outLen) {
    out[0] = '\0';
    return false;
  }
  return true;
}

inline bool homeBackgroundReusable(const char* savedKey, const char* currentKey) {
  return savedKey[0] != '\0' && strcmp(savedKey, currentKey) == 0;
}
