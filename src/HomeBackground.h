#pragma once

#include <HalMemory.h>

class Bitmap;
class GfxRenderer;

// A faded copy of the last sleep image, shown behind the Title List home.
namespace home_background {
// Saves a light dot-pattern rendition of a full-screen sleep bitmap; the frame buffer is left as it was.
void captureFromBitmap(const GfxRenderer& renderer, const Bitmap& bitmap, int x, int y, float cropX, float cropY,
                       bool inverted);
// Loads the saved copy into PSRAM; empty when off, missing, or out of memory.
HalMemory::PsramBuffer load(const GfxRenderer& renderer);
}  // namespace home_background
