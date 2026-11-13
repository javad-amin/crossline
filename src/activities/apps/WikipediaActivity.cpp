#include "WikipediaActivity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {
constexpr const char* kWikiDir = "/.crosspoint/wiki";
constexpr size_t kMaxResponseBytes = 48 * 1024;
constexpr size_t kMaxExtractChars = 20000;

bool ensureDir(const char* path) {
  if (Storage.exists(path)) return true;
  return Storage.mkdir(path);
}

std::string trimCopy(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
}  // namespace

WikipediaActivity::WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Wikipedia", renderer, mappedInput) {}

void WikipediaActivity::onEnter() {
  Activity::onEnter();
  ensureDir("/.crosspoint");
  ensureDir(kWikiDir);
  promptSearch();
}

std::string WikipediaActivity::urlEncodeComponent(const std::string& s) const {
  std::string out;
  char buf[4];
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else if (c == ' ') {
      out += "%20";
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

std::string WikipediaActivity::sanitizeFilename(const std::string& title) const {
  std::string out;
  out.reserve(title.size());
  for (unsigned char c : title) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
        c == '_') {
      out += static_cast<char>(c);
    } else {
      out += '_';
    }
  }
  auto t = trimCopy(out);
  if (t.empty()) t = "article";
  if (t.size() > 60) t.resize(60);
  return t;
}

void WikipediaActivity::showError(const std::string& message) {
  state = State::Error;
  errorMessage = message;
  needsRedraw = true;
  requestUpdate();
}

void WikipediaActivity::promptSearch() {
  if (WiFi.status() != WL_CONNECTED) {
    state = State::EnterQuery;
    wifiSelectionPending = true;
    auto wifi = std::make_unique<WifiSelectionActivity>(renderer, mappedInput);
    startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
      wifiSelectionPending = false;
      if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
        finish();
        return;
      }
      promptSearch();
    });
    return;
  }

  state = State::EnterQuery;
  needsRedraw = true;
  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Wikipedia", currentQuery, 80);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    const auto q = trimCopy(std::get<KeyboardResult>(result.data).text);
    if (q.empty()) {
      finish();
      return;
    }
    currentQuery = q;
    startSearch(q);
  });
}

void WikipediaActivity::startSearch(const std::string& query) {
  state = State::Searching;
  statusLine = "Searching...";
  needsRedraw = true;
  requestUpdate(true);

  resultTitles.clear();
  resultDescs.clear();
  selectedResult = 0;

  if (WiFi.status() != WL_CONNECTED) {
    showError("WiFi not connected");
    return;
  }

  std::string body;
  body.reserve(4096);
  const std::string url =
      "https://en.wikipedia.org/w/api.php?action=opensearch&search=" + urlEncodeComponent(query) +
      "&limit=8&namespace=0&format=json";
  const bool ok = HttpDownloader::fetchUrl(
      url,
      [&](const uint8_t* data, size_t len) {
        if (body.size() + len > kMaxResponseBytes) {
          body.append(reinterpret_cast<const char*>(data), kMaxResponseBytes - body.size());
          return false;
        }
        body.append(reinterpret_cast<const char*>(data), len);
        return true;
      },
      "", "", std::vector<HttpDownloader::Header>{});

  if (!ok || body.empty()) {
    showError("Search failed");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err || !doc.is<JsonArray>()) {
    showError("Search parse error");
    return;
  }
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() < 2 || !arr[1].is<JsonArray>()) {
    showError("No results");
    return;
  }
  JsonArray titles = arr[1].as<JsonArray>();
  JsonArray descs = (arr.size() >= 3 && arr[2].is<JsonArray>()) ? arr[2].as<JsonArray>() : JsonArray();
  for (size_t i = 0; i < titles.size(); ++i) {
    const char* t = titles[i] | "";
    if (!t || !*t) continue;
    resultTitles.emplace_back(t);
    if (i < descs.size()) {
      const char* d = descs[i] | "";
      resultDescs.emplace_back(d ? d : "");
    } else {
      resultDescs.emplace_back();
    }
  }
  if (resultTitles.empty()) {
    showError("No results");
    return;
  }
  state = State::Results;
  needsRedraw = true;
  requestUpdate();
}

void WikipediaActivity::fetchArticle(int index) {
  if (index < 0 || index >= static_cast<int>(resultTitles.size())) return;
  if (WiFi.status() != WL_CONNECTED) {
    showError("WiFi not connected");
    return;
  }
  state = State::Fetching;
  statusLine = "Fetching...";
  needsRedraw = true;
  requestUpdate(true);

  const std::string title = resultTitles[index];
  std::string body;
  body.reserve(8192);
  const std::string url =
      "https://en.wikipedia.org/w/api.php?action=query&prop=extracts&explaintext=1&exsectionformat=plain&redirects=1"
      "&format=json&formatversion=2&titles=" +
      urlEncodeComponent(title);
  const bool ok = HttpDownloader::fetchUrl(
      url,
      [&](const uint8_t* data, size_t len) {
        if (body.size() + len > kMaxResponseBytes) {
          body.append(reinterpret_cast<const char*>(data), kMaxResponseBytes - body.size());
          return false;
        }
        body.append(reinterpret_cast<const char*>(data), len);
        return true;
      },
      "", "", std::vector<HttpDownloader::Header>{});

  if (!ok || body.empty()) {
    showError("Fetch failed");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    showError("Article parse error");
    return;
  }
  JsonArray pages = doc["query"]["pages"].as<JsonArray>();
  if (pages.isNull() || pages.size() == 0) {
    showError("Article not found");
    return;
  }
  JsonObject page = pages[0].as<JsonObject>();
  if (page["missing"]) {
    showError("Article not found");
    return;
  }
  const char* extract = page["extract"] | "";
  const char* pageTitle = page["title"] | title.c_str();
  std::string text = pageTitle ? std::string(pageTitle) + "\n\n" : std::string();
  if (extract) text += extract;
  if (text.size() > kMaxExtractChars) text.resize(kMaxExtractChars);

  const std::string path = std::string(kWikiDir) + "/" + sanitizeFilename(pageTitle ? pageTitle : title) + ".txt";
  // Overwrite
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  if (!Storage.writeFile(path.c_str(), String(text.c_str()))) {
    showError("Save failed");
    return;
  }

  state = State::Opening;
  needsRedraw = true;
  requestUpdate();
  auto reader = ReaderActivity::create(renderer, mappedInput, path, false);
  if (!reader) {
    showError("Could not open reader");
    return;
  }
  startActivityForResult(std::move(reader), [this](const ActivityResult&) {
    // After reading, return to results
    state = State::Results;
    needsRedraw = true;
    requestUpdate();
  });
}

void WikipediaActivity::loop() {
  if (wifiSelectionPending) return;

  if (state == State::Error) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      finish();
    }
    return;
  }

  if (state == State::Results) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    const int count = static_cast<int>(resultTitles.size());
    if (count > 0) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
        selectedResult = (selectedResult + count - 1) % count;
        needsRedraw = true;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
        selectedResult = (selectedResult + 1) % count;
        needsRedraw = true;
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        fetchArticle(selectedResult);
        return;
      }
      int tx = 0, ty = 0;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        const auto& metrics = UITheme::getInstance().getMetrics();
        const int top = metrics.topPadding + metrics.headerHeight + 8;
        const int rh = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) * 2 + 10);
        const int margin = std::max(12, metrics.contentSidePadding);
        if (tx >= margin && tx < renderer.getScreenWidth() - margin && ty >= top) {
          const int idx = (ty - top) / rh;
          if (idx >= 0 && idx < count) {
            selectedResult = idx;
            fetchArticle(idx);
            return;
          }
        }
      }
    }
  }
}

void WikipediaActivity::render(RenderLock&&) {
  if (!needsRedraw) return;
  needsRedraw = false;
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& theme = UITheme::getInstance();
  const int margin = std::max(12, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, metrics.topPadding, "Wikipedia", true, EpdFontFamily::BOLD);

  if (state == State::Searching || state == State::Fetching || state == State::Opening || state == State::Idle) {
    const char* msg = statusLine.empty() ? "Loading..." : statusLine.c_str();
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, msg, true);
    GUI.drawButtonHints(renderer, "Back", "", "", "");
    renderer.displayBuffer();
    return;
  }

  if (state == State::Error) {
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, "Error", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 60, errorMessage.c_str(), true);
    GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), "", "");
    renderer.displayBuffer();
    return;
  }

  if (state == State::Results) {
    int y = metrics.topPadding + metrics.headerHeight + 8;
    const int rh = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) * 2 + 10);
    const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 6;
    for (int i = 0; i < static_cast<int>(resultTitles.size()); ++i) {
      if (y + rh > bottom) break;
      const bool sel = (i == selectedResult);
      const int w = renderer.getScreenWidth() - margin * 2;
      renderer.drawRect(margin, y, w, rh - 4, sel ? 2 : 1, true);
      if (sel) renderer.fillRect(margin + 2, y + 2, 4, rh - 8);
      const std::string& t = resultTitles[i];
      const std::string& d = (i < static_cast<int>(resultDescs.size())) ? resultDescs[i] : std::string();
      renderer.drawText(UI_10_FONT_ID, margin + 12, y + 4, t.c_str(), true, EpdFontFamily::BOLD);
      if (!d.empty()) renderer.drawText(SMALL_FONT_ID, margin + 12, y + 4 + renderer.getLineHeight(UI_10_FONT_ID), d.c_str(), true);
      y += rh;
    }
    GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    renderer.displayBuffer();
    return;
  }

  // EnterQuery state is usually covered by keyboard activity, but draw fallback
  renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, "Search Wikipedia", true);
  GUI.drawButtonHints(renderer, tr(STR_BACK), "", "", "");
  renderer.displayBuffer();
}
