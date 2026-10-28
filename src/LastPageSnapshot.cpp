#include "LastPageSnapshot.h"

#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace {
constexpr char LAST_PAGE_FILE[] = "/.crosspoint/last_page.bin";

// Allocated on first use and kept: a reader page turn must not churn a 48 KB block.
HalMemory::PsramBuffer snapshot;
size_t snapshotSize = 0;
bool remembered = false;
}  // namespace

namespace last_page {

void begin() { remembered = false; }

void remember(const GfxRenderer& renderer) {
  const uint8_t* frame = renderer.getFrameBuffer();
  const size_t total = renderer.getBufferSize();
  if (!frame || total == 0) return;
  if (!snapshot || snapshotSize != total) {
    snapshot = HalMemory::allocatePsram(total);
    snapshotSize = snapshot ? total : 0;
    if (!snapshot) return;
  }
  memcpy(snapshot.get(), frame, total);
  remembered = true;
}

void persist(const GfxRenderer& renderer) {
  if (!remembered || snapshotSize != renderer.getBufferSize()) {
    Storage.remove(LAST_PAGE_FILE);
    return;
  }
  HalFile file;
  bool ok = Storage.openFileForWrite("LPG", LAST_PAGE_FILE, file);
  if (ok) {
    ok = file.write(snapshot.get(), snapshotSize) == snapshotSize;
    file.close();
  }
  if (!ok) {
    LOG_ERR("LPG", "Failed to save %s", LAST_PAGE_FILE);
    Storage.remove(LAST_PAGE_FILE);
  }
}

bool load(const GfxRenderer& renderer) {
  uint8_t* frame = renderer.getFrameBuffer();
  const size_t total = renderer.getBufferSize();
  if (!frame || !Storage.exists(LAST_PAGE_FILE)) return false;
  HalFile file;
  if (!Storage.openFileForRead("LPG", LAST_PAGE_FILE, file)) return false;
  const bool ok = file.fileSize() == total && file.read(frame, total) == static_cast<int>(total);
  file.close();
  if (!ok) LOG_ERR("LPG", "Ignoring unreadable %s", LAST_PAGE_FILE);
  return ok;
}

}  // namespace last_page
