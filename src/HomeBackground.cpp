#include "HomeBackground.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "HomeBackgroundKey.h"

namespace {
constexpr char HOME_BACKGROUND_FILE[] = "/.crosspoint/home_background.bin";
constexpr char HOME_BACKGROUND_KEY_FILE[] = "/.crosspoint/home_background.key";

char sourcePath[256] = "";
uint32_t sourceSize = 0;
uint32_t sourceTime = 0;

// Empty when there is no saved key or it can't be read.
void readSavedKey(char* out, const size_t len) {
  out[0] = '\0';
  HalFile file;
  if (!Storage.exists(HOME_BACKGROUND_KEY_FILE) || !Storage.openFileForRead("HBG", HOME_BACKGROUND_KEY_FILE, file)) {
    return;
  }
  const int n = file.read(reinterpret_cast<uint8_t*>(out), len - 1);
  out[n > 0 ? n : 0] = '\0';
}

void writeSavedKey(const char* key) {
  HalFile file;
  if (!key[0] || !Storage.openFileForWrite("HBG", HOME_BACKGROUND_KEY_FILE, file) ||
      file.write(reinterpret_cast<const uint8_t*>(key), strlen(key)) != strlen(key)) {
    file.close();
    Storage.remove(HOME_BACKGROUND_KEY_FILE);
  }
}

// Full-contrast 1-bit rendition: black stays black, the two grays use a 4x4 ordered dither (~2/3 and ~1/3 ink).
bool isDot(const uint8_t level, const int x, const int y) {
  static constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  static constexpr uint8_t INK_SIXTEENTHS[4] = {16, 11, 5, 0};
  return BAYER[y & 3][x & 3] < INK_SIXTEENTHS[level & 3];
}

// Mirrors GfxRenderer::drawBitmap's crop/scale mapping.
bool drawFaded(const GfxRenderer& renderer, const Bitmap& bitmap, const int x, const int y, const float cropX,
               const float cropY, const bool inverted) {
  if (bitmap.rewindToData() != BmpReaderError::Ok) return false;
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int cropPixX = static_cast<int>(std::floor(bitmap.getWidth() * cropX / 2.0f));
  const int cropPixY = static_cast<int>(std::floor(bitmap.getHeight() * cropY / 2.0f));
  const float croppedW = (1.0f - cropX) * static_cast<float>(bitmap.getWidth());
  const float croppedH = (1.0f - cropY) * static_cast<float>(bitmap.getHeight());
  float scale = 1.0f;
  if (croppedW > 0.0f && croppedH > 0.0f) {
    scale = std::min(1.0f, std::min(screenW / croppedW, screenH / croppedH));
  }

  const int outputRowSize = (bitmap.getWidth() + 3) / 4;
  auto scratch = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>(outputRowSize + bitmap.getRowBytes()));
  if (!scratch) {
    LOG_ERR("HBG", "No memory for bitmap rows");
    return false;
  }
  uint8_t* outputRow = scratch.get();
  uint8_t* rowBytes = scratch.get() + outputRowSize;

  for (int bmpY = 0; bmpY < bitmap.getHeight() - cropPixY; ++bmpY) {
    int screenY = -cropPixY + (bitmap.isTopDown() ? bmpY : bitmap.getHeight() - 1 - bmpY);
    screenY = static_cast<int>(std::floor(screenY * scale)) + y;
    if (screenY >= screenH) break;
    if (bitmap.readNextRow(outputRow, rowBytes) != BmpReaderError::Ok) return false;
    if (screenY < 0 || bmpY < cropPixY) continue;
    for (int bmpX = cropPixX; bmpX < bitmap.getWidth() - cropPixX; ++bmpX) {
      const int screenX = static_cast<int>(std::floor((bmpX - cropPixX) * scale)) + x;
      if (screenX >= screenW) break;
      if (screenX < 0) continue;
      uint8_t level = outputRow[bmpX / 4] >> (6 - ((bmpX * 2) % 8)) & 0x3;
      if (inverted) level = 3 - level;
      if (isDot(level, screenX, screenY)) renderer.drawPixel(screenX, screenY);
    }
  }
  return true;
}
}  // namespace

namespace home_background {

void setSource(const char* imagePath, const uint32_t fileSize, const uint32_t modifiedTime) {
  snprintf(sourcePath, sizeof(sourcePath), "%s", imagePath ? imagePath : "");
  sourceSize = fileSize;
  sourceTime = modifiedTime;
}

void captureFromBitmap(const GfxRenderer& renderer, const Bitmap& bitmap, const int x, const int y, const float cropX,
                       const float cropY, const bool inverted) {
  if (!SETTINGS.homeBackground) return;
  char key[HOME_BACKGROUND_KEY_LEN];
  makeHomeBackgroundKey(key, sizeof(key), sourcePath, sourceSize, sourceTime, inverted);
  sourcePath[0] = '\0';
  {
    char savedKey[HOME_BACKGROUND_KEY_LEN];
    readSavedKey(savedKey, sizeof(savedKey));
    if (homeBackgroundReusable(savedKey, key) && Storage.exists(HOME_BACKGROUND_FILE)) {
      LOG_DBG("HBG", "Reusing home background for %s", key);
      return;
    }
  }
  Storage.remove(HOME_BACKGROUND_KEY_FILE);
  uint8_t* frame = renderer.getFrameBuffer();
  const size_t total = renderer.getBufferSize();
  // PSRAM keeps the caller's frame safe while the faded copy is drawn into the frame buffer.
  auto saved = HalMemory::allocatePsram(total);
  if (!frame || !saved) {
    LOG_ERR("HBG", "No PSRAM to capture home background (%u bytes)", static_cast<unsigned>(total));
    return;
  }
  memcpy(saved.get(), frame, total);
  renderer.clearScreen();
  bool ok = drawFaded(renderer, bitmap, x, y, cropX, cropY, inverted);

  if (ok) {
    HalFile file;
    ok = Storage.openFileForWrite("HBG", HOME_BACKGROUND_FILE, file);
    if (ok) {
      ok = file.write(frame, total) == total;
      file.close();
    }
  }
  if (!ok) {
    LOG_ERR("HBG", "Failed to save %s", HOME_BACKGROUND_FILE);
    Storage.remove(HOME_BACKGROUND_FILE);
  } else {
    writeSavedKey(key);
  }
  memcpy(frame, saved.get(), total);
}

HalMemory::PsramBuffer load(const GfxRenderer& renderer) {
  if (!SETTINGS.homeBackground || !Storage.exists(HOME_BACKGROUND_FILE)) return {};
  const size_t total = renderer.getBufferSize();
  auto buffer = HalMemory::allocatePsram(total);
  if (!buffer) {
    LOG_ERR("HBG", "No PSRAM for home background (%u bytes)", static_cast<unsigned>(total));
    return {};
  }
  HalFile file;
  if (!Storage.openFileForRead("HBG", HOME_BACKGROUND_FILE, file)) return {};
  // A size mismatch means a different panel or orientation: skip rather than draw garbage.
  const bool ok = file.fileSize() == total && file.read(buffer.get(), total) == static_cast<int>(total);
  file.close();
  if (!ok) {
    LOG_ERR("HBG", "Ignoring unreadable %s", HOME_BACKGROUND_FILE);
    return {};
  }
  return buffer;
}

}  // namespace home_background
