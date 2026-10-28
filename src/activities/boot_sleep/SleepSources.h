#pragma once

#include <cstdint>
#include <strings.h>

// Which sleep-image files and folders exist in the SD root, learned from one
// directory listing. Probing each path separately costs a full root-directory
// scan per missing name, which is slow on a card whose root has grown large.
class SleepSources {
 public:
  enum Source : uint8_t {
    SLEEP_BMP,           // /sleep.bmp
    SLEEP_HIDDEN_DIR,    // /.sleep
    SLEEP_DIR,           // /sleep
    OVERLAY_BMP,         // /sleep-overlay.bmp
    OVERLAY_PNG,         // /sleep-overlay.png
    OVERLAY_HIDDEN_DIR,  // /.sleep-overlay
    OVERLAY_DIR,         // /sleep-overlay
    SOURCE_COUNT
  };

  // Used when the root can't be listed: every source is still probed.
  static SleepSources unknown() {
    SleepSources sources;
    sources.present = (1u << SOURCE_COUNT) - 1;
    return sources;
  }

  void noteRootEntry(const char* name, const bool isDirectory) {
    for (uint8_t i = 0; i < SOURCE_COUNT; i++) {
      if (ENTRIES[i].isDirectory == isDirectory && strcasecmp(name, ENTRIES[i].name) == 0) {
        present |= static_cast<uint8_t>(1u << i);
      }
    }
  }

  bool has(const Source source) const { return (present >> source) & 1u; }
  bool any() const { return present != 0; }

 private:
  struct Entry {
    const char* name;
    bool isDirectory;
  };
  static constexpr Entry ENTRIES[SOURCE_COUNT] = {
      {"sleep.bmp", false},         {".sleep", true},         {"sleep", true},         {"sleep-overlay.bmp", false},
      {"sleep-overlay.png", false}, {".sleep-overlay", true}, {"sleep-overlay", true},
  };

  uint8_t present = 0;
};
