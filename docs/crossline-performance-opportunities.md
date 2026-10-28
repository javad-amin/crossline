# Crossline: speed and responsiveness opportunities

Investigation of the `stable` branch (CrossPoint 1.6.5 + Crossline changes) on the Xteink X4 Pro.
Measured numbers come from device serial logs captured on 2026-10-28; everything else is derived
from reading the code and is an estimate, not a measurement.

## What we already learned on the device

| Measurement | Value | Notes |
| --- | --- | --- |
| Raw SD sector read (1 sector / 8 sectors) | 0.5 ms / 2.0 ms | SDMMC, **1-bit bus at 20 MHz** |
| File lookup before card cleanup | ~445 ms per lookup | Root directory bloated with ~14,000 deleted entries |
| Wake → page turns accepted (before cleanup) | ~7–10 s | One 9.5 s main-loop block after the first page |
| Wake after cleanup | "instant" (user report) | Root directory rebuilt, clutter removed |

The single biggest problem was the SD card's top-level directory, not the firmware. It is fixed
for now, but it will come back if scripts write and delete files in the card root again (see
[Keep the card healthy](#keep-the-card-healthy)).

## Prioritized opportunities

Effort: S = small/local change, M = a few files, L = new subsystem. Risk is about regressions on
the panel (ghosting), data safety, or memory.

### 1. Make idle prewarm interruptible — biggest responsiveness win (M, low–medium risk)

- **Now:** 400 ms after a page is drawn, the reader loads the *next* page, scans its glyphs and
  prewarms fonts synchronously inside `loop()` while holding the render lock
  ([EpubReaderActivity.cpp:389](../src/activities/reader/EpubReaderActivity.cpp#L389)). Input
  handling for that loop iteration only runs afterwards
  ([line 460](../src/activities/reader/EpubReaderActivity.cpp#L460)). There is no pending-input
  check or cancellation. On the device this blocked input for **3.6 s** (and 9.5 s total
  together with the post-render progress save).
- **Change:** handle input first; split prewarm into small units (one style / one batch of
  glyphs / one kerning load) and stop as soon as a button or touch is pending. Keep the
  deserialized next page so the real page turn doesn't load it again.
- **Benefit:** page turns and menu presses are never swallowed by background work.

### 2. Run the SD card at 40 MHz (S, medium risk — needs hardware testing)

- **Now:** [SdmmcBlockDevice.cpp:25](../freeink-sdk/libs/hardware/SDCardManager/src/SdmmcBlockDevice.cpp#L25)
  sets `SDMMC_FREQ_DEFAULT` with the comment "40 MHz", but ESP-IDF's default is 20 MHz; the
  device reported `real=20000kHz`. The board config comment says the OEM firmware runs this slot
  at 40 MHz ([BoardConfig.h:1667](../freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h#L1667)).
  The bus is 1-bit (D1–D3 not wired), so only the clock can be raised.
- **Change:** try `SDMMC_FREQ_HIGHSPEED` (40 MHz), keep 20 MHz as a fallback if mount or the
  sector-0 read fails.
- **Benefit:** up to ~2× raw read/write throughput for every book, font, cache and image read.
  **Risk:** signal integrity on some units/cards; must be soak-tested (open many books, sleep/wake
  cycles) before keeping it.

### 3. Cache kerning data and font metadata in PSRAM (M, low risk)

- **Now:** each SD font style loads its kerning class maps and ligatures lazily
  ([SdCardFont.cpp:247](../lib/EpdFont/SdCardFont.cpp#L247)); the per-page "mini kern" matrix is
  rebuilt with one seek + one row read per left class
  ([SdCardFont.cpp:446](../lib/EpdFont/SdCardFont.cpp#L446)). Everything is released whenever the
  reader exits ([ReaderActivity.cpp:96](../src/activities/reader/ReaderActivity.cpp#L96)). Device
  logs showed these loads at roughly 1 s each on the bloated card.
- **Change:** load each used style's full kerning matrix once into PSRAM (~36 KB/style for
  Literata, ~144 KB for four styles out of 8 MB) and build mini matrices from memory; keep caches
  across reader → Home → reader instead of releasing them unconditionally.
- **Benefit:** removes repeated small SD reads on page turns and when reopening a book.

### 4. Save less often, and only when something changed (S–M, low risk)

- **Progress:** written after every page turn (temp file + write + flush + close + remove +
  rename) under the render lock
  ([EpubReaderActivity.cpp:1454](../src/activities/reader/EpubReaderActivity.cpp#L1454),
  [ProgressFile.h:31](../src/activities/reader/ProgressFile.h#L31)). Debounce (e.g. a few seconds
  or every N pages) and always force a save on exit, sleep, sync and jumps.
- **App state on sleep:** saved two or three times in one sleep
  ([main.cpp:274](../src/main.cpp#L274), [ReaderActivity.cpp:103](../src/activities/reader/ReaderActivity.cpp#L103),
  [SleepActivity.cpp:464](../src/activities/boot_sleep/SleepActivity.cpp#L464)). Consolidate into
  one final checkpoint.
- **Per-book settings:** rewritten on every reader exit when a book has overrides, even if
  unchanged ([BookReaderSettings.cpp:129](../src/BookReaderSettings.cpp#L129)). Compare with the
  last saved values first.
- **Recent books:** always re-saved when a book is opened, even if it is already first with the
  same metadata ([RecentBooksStore.cpp:43](../src/RecentBooksStore.cpp#L43)). Skip identical
  updates.
- **Control center tiles:** night mode / orientation / touch save settings immediately
  ([FrontlightPanelActivity.cpp:148](../src/activities/util/FrontlightPanelActivity.cpp#L148)).
  Mark dirty and save once when the panel closes.

### 5. Faster wake into the book (M, low–medium risk)

- **Font discovery on every boot:** all of `/.fonts` and `/fonts` are scanned on each wake,
  including families that aren't used
  ([SdCardFontSystem.cpp:71](../src/SdCardFontSystem.cpp#L71),
  [SdCardFontRegistry.cpp:392](../lib/EpdFont/SdCardFontRegistry.cpp#L392)). Cache the registry
  in a small manifest and only rescan when the font picker opens, fonts are uploaded, or after
  USB Drive mode.
- **Possible double font load:** boot loads the global font, then the reader applies per-book
  settings and may load a different family
  ([SdCardFontSystem.cpp:81](../src/SdCardFontSystem.cpp#L81),
  [ReaderActivity.cpp:71](../src/activities/reader/ReaderActivity.cpp#L71)). When resuming into a
  book, resolve the book's settings first.
- **Stores not needed for reading** (KOReader, OPDS) are loaded before the first page
  ([main.cpp:416](../src/main.cpp#L416)). Load them lazily.
- **CSS rules are loaded and discarded** even when the section cache is valid
  ([Epub.cpp:441](../lib/Epub/Epub.cpp#L441)). Defer until a rebuild needs them.

### 6. Show the last page instantly on wake (M, medium risk)

- The Transparent sleep screen and Quick Resume already keep a full page image on SD
  (`/.crosspoint/last_page.bin`). On wake, the panel already shows that page; the reader then
  re-renders it from scratch. X4 Pro does not get the "fast first page" path that X3 has
  ([main.cpp:513](../src/main.cpp#L513)).
- **Option:** accept input immediately after wake and let the first page turn replace the image,
  or enable the fast initial refresh on X4 Pro after validating ghosting on the panel.

### 7. Sleep screen work (S–M, low risk)

- The sleep image is decoded three times (BW + two gray planes)
  ([SleepActivity.cpp:640](../src/activities/boot_sleep/SleepActivity.cpp#L640)) plus a fourth
  pass for the Home background ([HomeBackground.cpp:70](../src/HomeBackground.cpp#L70)). Cache the
  Home background by image file + size + date and skip the capture when the image hasn't changed.
- Random image selection validates every file in `/sleep` twice and then waits `delay(100)`
  ([SleepActivity.cpp:430](../src/activities/boot_sleep/SleepActivity.cpp#L430),
  [line 602](../src/activities/boot_sleep/SleepActivity.cpp#L602)). Cache a list of valid images;
  check whether the delay is still needed.
- Transparent sleep from the reader writes the last page to SD and immediately reads it back
  ([LastPageSnapshot.cpp](../src/LastPageSnapshot.cpp)). Use the in-memory copy when it's
  available, and only write the file for later.

### 8. Home screen (S–M, low risk)

- Opening Home loads the latest book's full EPUB metadata just to show its progress
  percentage, walking every spine entry
  ([CoverGridHomeUi.cpp:54](../src/components/CoverGridHomeUi.cpp#L54),
  [BookProgress.cpp:19](../src/util/BookProgress.cpp#L19)). Store the percentage alongside
  progress (or in recents) so Home doesn't need the EPUB at all.
- Home checks every recent book path on SD on each entry
  ([HomeActivity.cpp:39](../src/activities/home/HomeActivity.cpp#L39)); reuse the result within
  a session.

### 9. Reader rendering (M–L, medium risk)

- Chapter title in the status bar reads spine/TOC entries from SD on every page (about eight
  seeks) ([EpubReaderActivity.cpp:1818](../src/activities/reader/EpubReaderActivity.cpp#L1818)).
  Cache the current chapter title — low risk.
- Anti-aliased text rendering re-renders the page for each gray plane and, on this panel's
  non-tiled path, allocates/copies a framebuffer snapshot each time
  ([GfxRenderer.cpp:2375](../lib/GfxRenderer/GfxRenderer.cpp#L2375)). With PSRAM, gray planes could
  be rendered once into reusable buffers (~96 KB).
- An image page forces a HALF (cleaning) refresh on the next text page
  ([EpubReaderActivity.cpp:1625](../src/activities/reader/EpubReaderActivity.cpp#L1625)). Worth
  revisiting only with ghosting tests.

### 10. Input latency details (S, low risk)

- After 3 s idle the loop sleeps up to 50 ms between polls and only wakes early for buttons, not
  touch ([main.cpp:818](../src/main.cpp#L818), [HalGPIO.cpp:161](../lib/hal/HalGPIO.cpp#L161)).
  Waking on the touch interrupt would cut tap latency.
- With **Double Click Power Light** enabled, a single power click waits 500 ms for a possible
  second click before acting ([main.cpp:63](../src/main.cpp#L63)). This affects the
  power-to-Home shortcut. Turn the setting off or shorten the window.

## Settings you can change today

| Setting | Effect |
| --- | --- |
| Text Anti-Aliasing → Off | Removes the two gray passes on text pages (images still use gray). Faster turns, slightly less smooth text. |
| Refresh frequency → higher / Never | Fewer HALF cleaning refreshes; more ghosting between cleanups. |
| Double Click Power Light → Off | Power-button actions (Home, Sleep) react immediately instead of after 500 ms. |
| Long-press page turn → Off | Side buttons turn on press instead of release. |
| Home Background → Off | Skips the extra sleep-image pass and the 48 KB file write on each sleep. |
| Sleep screen → Quick Resume or Blank | Fastest sleep; no image decoding. |
| Fewer SD font families installed | Less to scan on every wake (until discovery is cached). |

## Keep the card healthy

- Don't run scripts with the card's top folder as their working directory; temporary and
  `._*` files there grow the root directory permanently on exFAT.
- Keep `.metadata_never_index` in the card root and run `dot_clean /Volumes/X4PRO` before
  ejecting on macOS.
- If wake gets slow again, the definitive fix is: back up, erase the card (exFAT), copy back.

## Suggested order

1. Interruptible idle prewarm (#1) — fixes "device ignores me after a page appears".
2. 40 MHz SD clock experiment (#2) — cheap to try, large potential gain.
3. Save-less-often batch (#4) and chapter-title cache (#9 first bullet) — small, safe wins.
4. Kerning/font caching in PSRAM (#3) and cached font discovery (#5).
5. Home progress without EPUB load (#8) and sleep-screen caching (#7).

## How to verify each change

- Build with `-DLOG_LEVEL=2` and use the existing timing logs: `Page render: …` and
  `Rendered page in …` ([EpubReaderActivity.cpp:1705](../src/activities/reader/EpubReaderActivity.cpp#L1705)),
  `Idle prewarm: page N in …ms`, and `New max loop duration`.
- Compare: cold wake to first page, wake to first accepted page turn, warm page turns, opening a
  book with bold/italic text, Home entry, and going to sleep.
- Check ghosting and typography on the device in all four orientations for any change touching
  refresh or grayscale.
