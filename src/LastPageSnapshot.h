#pragma once

class GfxRenderer;

// The last clean reader page, kept so the Transparent sleep screen can overlay
// the open book even when sleep starts from Home or another screen.
namespace last_page {
// Starts a reading session; nothing is persisted until a page is remembered.
void begin();
// Copies the frame buffer right after a page is drawn (PSRAM only).
void remember(const GfxRenderer& renderer);
// Saves the remembered page to SD, or drops a stale file if none was remembered.
void persist(const GfxRenderer& renderer);
// Loads the saved page into the frame buffer; false when missing or mismatched.
bool load(const GfxRenderer& renderer);
}  // namespace last_page
