#include "GamesActivity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "../apps/CalculatorActivity.h"
#include "../apps/StopwatchActivity.h"
#include "../apps/NotesActivity.h"
#include "../apps/WikipediaActivity.h"
#include "ConnectFourActivity.h"
#include "MappedInputManager.h"
#include "MinesweeperActivity.h"
#include "NonogramsActivity.h"
#include "SnakeActivity.h"
#include "SudokuActivity.h"
#include "chess/ChessActivity.h"
#include "components/UITheme.h"
#include "components/icons/chess_icon.h"
#include "components/icons/connect_four_icon.h"
#include "components/icons/minesweeper_icon.h"
#include "components/icons/nonograms_icon.h"
#include "components/icons/snake_icon.h"
#include "components/icons/sudoku_icon.h"
#include "fontIds.h"

namespace {
constexpr char kPrefsPath[] = "/.crosspoint/games_hub.json";

const char* labelChess() { return "Chess"; }
const char* labelCalc() { return "Calculator"; }
const char* labelStopwatch() { return "Stopwatch"; }

bool containsStr(const std::vector<std::string>& v, const char* id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}
void removeStr(std::vector<std::string>& v, const char* id) {
  v.erase(std::remove(v.begin(), v.end(), id), v.end());
}
}

const GamesActivity::Item GamesActivity::kGames[] = {
    {"chess", nullptr, ChessIcon.data()},
    {"connectfour", nullptr, ConnectFourIcon.data()},
    {"minesweeper", nullptr, MinesweeperIcon.data()},
    {"nonograms", nullptr, NonogramsIcon.data()},
    {"snake", nullptr, SnakeIcon.data()},
    {"sudoku", nullptr, SudokuIcon.data()},
};
const GamesActivity::Item GamesActivity::kApps[] = {
    {"calculator", nullptr, nullptr},
    {"stopwatch", nullptr, nullptr},
    {"notes", nullptr, nullptr},
    {"wikipedia", nullptr, nullptr},
};

GamesActivity::GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Games", renderer, mappedInput) {}

void GamesActivity::onEnter() {
  Activity::onEnter();
  loadPrefs();
  rebuildLists();
  requestUpdate();
}

bool GamesActivity::isPinned(Tab tab, const char* id) const {
  return containsStr(tab == Tab::Games ? pinnedGames : pinnedApps, id);
}
bool GamesActivity::isHidden(Tab tab, const char* id) const {
  return containsStr(tab == Tab::Games ? hiddenGames : hiddenApps, id);
}
void GamesActivity::setPinned(Tab tab, const char* id, bool pinned) {
  auto& v = tab == Tab::Games ? pinnedGames : pinnedApps;
  removeStr(v, id);
  if (pinned) v.insert(v.begin(), id);
}
void GamesActivity::setHidden(Tab tab, const char* id, bool hidden) {
  auto& v = tab == Tab::Games ? hiddenGames : hiddenApps;
  removeStr(v, id);
  if (hidden) v.push_back(id);
  // also unpin when hiding
  if (hidden) setPinned(tab, id, false);
}

void GamesActivity::loadPrefs() {
  pinnedGames.clear(); pinnedApps.clear(); hiddenGames.clear(); hiddenApps.clear();
  if (!Storage.exists(kPrefsPath)) return;
  std::unique_ptr<char[]> buf(new (std::nothrow) char[2048]);
  if (!buf) return;
  const size_t n = Storage.readFileToBuffer(kPrefsPath, buf.get(), 2047);
  if (n == 0) return;
  buf[n] = '\0';
  JsonDocument doc;
  if (deserializeJson(doc, buf.get())) return;
  auto loadArr = [&](const char* key, std::vector<std::string>& out) {
    JsonArray a = doc[key].as<JsonArray>();
    if (a.isNull()) return;
    for (JsonVariant v : a) { if (v.is<const char*>()) out.emplace_back(v.as<const char*>()); }
  };
  loadArr("pinnedGames", pinnedGames);
  loadArr("pinnedApps", pinnedApps);
  loadArr("hiddenGames", hiddenGames);
  loadArr("hiddenApps", hiddenApps);
  LOG_INF("GAMES", "Loaded prefs: hiddenGames=%d hiddenApps=%d", static_cast<int>(hiddenGames.size()), static_cast<int>(hiddenApps.size()));
}

void GamesActivity::savePrefs() const {
  JsonDocument doc;
  auto saveArr = [&](const char* key, const std::vector<std::string>& v) {
    JsonArray a = doc[key].to<JsonArray>();
    for (auto& s : v) a.add(s.c_str());
  };
  saveArr("pinnedGames", pinnedGames);
  saveArr("pinnedApps", pinnedApps);
  saveArr("hiddenGames", hiddenGames);
  saveArr("hiddenApps", hiddenApps);
  char out[2048];
  const size_t n = serializeJson(doc, out, sizeof(out));
  if (n > 0) {
    if (!Storage.writeFile(kPrefsPath, String(out))) {
      LOG_ERR("GAMES", "Failed to save prefs to %s", kPrefsPath);
    } else {
      LOG_INF("GAMES", "Saved prefs to %s", kPrefsPath);
    }
  }
}

void GamesActivity::rebuildLists() {
  visibleGames.clear();
  visibleApps.clear();
  auto build = [&](Tab tab, int count, const Item* items, std::vector<int>& out) {
    const auto& pinned = tab == Tab::Games ? pinnedGames : pinnedApps;
    // pinned first in pinned order
    for (const auto& id : pinned) {
      if (isHidden(tab, id.c_str())) continue;
      for (int i = 0; i < count; ++i) if (items[i].id == id || std::string(items[i].id) == id) { out.push_back(i); break; }
    }
    // rest in original order
    for (int i = 0; i < count; ++i) {
      if (isHidden(tab, items[i].id)) continue;
      if (isPinned(tab, items[i].id)) continue;
      out.push_back(i);
    }
  };
  build(Tab::Games, kGameCount, kGames, visibleGames);
  build(Tab::Apps, kAppCount, kApps, visibleApps);
  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, static_cast<int>(visibleGames.size()) - 1));
  appSelectedIndex = std::clamp(appSelectedIndex, 0, std::max(0, static_cast<int>(visibleApps.size()) - 1));
}

void GamesActivity::openSelectedIndex(int idx) {
  if (activeTab == Tab::Games) {
    if (idx < 0 || idx >= static_cast<int>(visibleGames.size())) return;
    const int gi = visibleGames[idx];
    switch (gi) {
      case 0: if (auto a = makeUniqueNoThrow<ChessActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 1: if (auto a = makeUniqueNoThrow<ConnectFourActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 2: if (auto a = makeUniqueNoThrow<MinesweeperActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 3: if (auto a = makeUniqueNoThrow<NonogramsActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 4: if (auto a = makeUniqueNoThrow<SnakeActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 5: if (auto a = makeUniqueNoThrow<SudokuActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      default: return;
    }
  } else {
    if (idx < 0 || idx >= static_cast<int>(visibleApps.size())) return;
    const int ai = visibleApps[idx];
    switch (ai) {
      case 0: if (auto a = makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 1: if (auto a = makeUniqueNoThrow<StopwatchActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 2: if (auto a = makeUniqueNoThrow<NotesActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      case 3: if (auto a = makeUniqueNoThrow<WikipediaActivity>(renderer, mappedInput)) startActivityForResult(std::move(a), [](const ActivityResult&){}); return;
      default: return;
    }
  }
}

void GamesActivity::openSelected() {
  openSelectedIndex(activeTab == Tab::Games ? selectedIndex : appSelectedIndex);
}

int GamesActivity::tabBarHeight() const { return std::max(36, renderer.getLineHeight(UI_10_FONT_ID) + 16); }
int GamesActivity::tabBarTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight;
}
int GamesActivity::rowHeight() const { return std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20) + 10; }
int GamesActivity::listTop() const { return tabBarTop() + tabBarHeight() + 8; }

void GamesActivity::drawTabs() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int top = tabBarTop();
  const int h = tabBarHeight();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int w = renderer.getScreenWidth() - margin * 2;
  const int gap = 10;
  const int tabW = (w - gap) / 2;
  const char* labels[] = {tr(STR_GAMES), "Apps"};

  for (int i = 0; i < 2; ++i) {
    const int x = margin + i * (tabW + gap);
    const bool active = static_cast<int>(activeTab) == i;
    renderer.drawRect(x, top, tabW, h);
    if (active) {
      renderer.fillRect(x + 2, top + 2, tabW - 4, h - 4);
    }
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, labels[i]);
    const int tx = x + (tabW - tw) / 2;
    const int ty = top + (h - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_12_FONT_ID, tx, ty, labels[i], !active);
  }
}


int GamesActivity::hiddenCount(Tab tab) const {
  return static_cast<int>((tab == Tab::Games ? hiddenGames : hiddenApps).size());
}

bool GamesActivity::hiddenButtonRect(int& x, int& y, int& w, int& h) const {
  if (showingHiddenList || hiddenCount(activeTab) <= 0) return false;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int margin = std::max(16, metrics.contentSidePadding);
  h = std::max(40, renderer.getLineHeight(UI_10_FONT_ID) + 16);
  w = renderer.getScreenWidth() - margin * 2;
  x = margin;
  y = renderer.getScreenHeight() - metrics.buttonHintsHeight - h - 6;
  return true;
}

int GamesActivity::mainListBottom() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
  int bx, by, bw, bh;
  if (hiddenButtonRect(bx, by, bw, bh)) bottom = by - 4;
  return bottom;
}

void GamesActivity::openHiddenList() {
  if (hiddenCount(activeTab) <= 0) return;
  showingHiddenList = true;
  hiddenListIndex = 0;
  requestUpdate();
}

void GamesActivity::closeHiddenList() {
  showingHiddenList = false;
  requestUpdate();
}

void GamesActivity::unhideHiddenIndex(int idx) {
  auto& hv = activeTab == Tab::Games ? hiddenGames : hiddenApps;
  if (idx < 0 || idx >= static_cast<int>(hv.size())) return;
  const std::string id = hv[idx];
  setHidden(activeTab, id.c_str(), false);
  savePrefs();
  rebuildLists();
  if (hiddenCount(activeTab) <= 0) showingHiddenList = false;
  else hiddenListIndex = std::min(hiddenListIndex, hiddenCount(activeTab) - 1);
  requestUpdate();
}

int GamesActivity::indexAtPoint(int x, int y) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int margin = std::max(16, metrics.contentSidePadding);
  const int rh = rowHeight();
  const int top = listTop();
  const int bottom = mainListBottom();
  const int count = showingHiddenList ? hiddenCount(activeTab)
                                      : (activeTab == Tab::Games ? static_cast<int>(visibleGames.size())
                                                                 : static_cast<int>(visibleApps.size()));
  if (x >= margin && x < renderer.getScreenWidth() - margin && y >= top && y < bottom) {
    const int idx = (y - top) / rh;
    if (idx >= 0 && idx < count) return idx;
  }
  return -1;
}

void GamesActivity::showItemOptions(Tab tab, int visibleIndex) {
  const bool onGames = tab == Tab::Games;
  const auto& vis = onGames ? visibleGames : visibleApps;
  if (visibleIndex < 0 || visibleIndex >= static_cast<int>(vis.size())) return;
  const Item& item = onGames ? kGames[vis[visibleIndex]] : kApps[vis[visibleIndex]];
  const bool pinned = isPinned(tab, item.id);
  const char* opts[] = {pinned ? "Unpin" : "Pin to top", "Hide", "Cancel"};
  std::string title;
  if (onGames) {
    const std::string id = item.id;
    if (id == "chess") title = "Chess";
    else if (id == "connectfour") title = tr(STR_CONNECT_FOUR);
    else if (id == "minesweeper") title = tr(STR_MINESWEEPER);
    else if (id == "nonograms") title = tr(STR_NONOGRAMS);
    else if (id == "snake") title = tr(STR_SNAKE);
    else title = tr(STR_SUDOKU);
  } else {
    const std::string id = item.id;
    if (id == "calculator") title = "Calculator";
    else if (id == "stopwatch") title = "Stopwatch";
    else if (id == "notes") title = "Notes";
    else title = "Wikipedia";
  }
  optionPopup.show("Options", title.c_str(), opts, 3, 0, [this, tab, id = item.id](int choice) {
    if (choice == 0) {
      setPinned(tab, id, !isPinned(tab, id));
      savePrefs();
      rebuildLists();
    } else if (choice == 1) {
      setHidden(tab, id, true);
      savePrefs();
      rebuildLists();
      LOG_INF("GAMES", "Hidden id=%s tab=%d hiddenGames=%d hiddenApps=%d", id, static_cast<int>(tab),
              static_cast<int>(hiddenGames.size()), static_cast<int>(hiddenApps.size()));
    }
    requestUpdate();
  });
  requestUpdate();
}

void GamesActivity::loop() {
  if (optionPopup.handleInput(mappedInput, [this]{ requestUpdate(); })) return;

  if (showingHiddenList) {
    const int hcount = hiddenCount(activeTab);
    if (hcount <= 0) { showingHiddenList = false; requestUpdate(); return; }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Right) {
      closeHiddenList();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      hiddenListIndex = (hiddenListIndex + hcount - 1) % hcount; requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      hiddenListIndex = (hiddenListIndex + 1) % hcount; requestUpdate(); return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      unhideHiddenIndex(hiddenListIndex);
      return;
    }
    int tx = 0, ty = 0;
    if (mappedInput.wasScreenTapped(tx, ty)) {
      const int idx = indexAtPoint(tx, ty);
      if (idx >= 0) { hiddenListIndex = idx; unhideHiddenIndex(idx); }
    }
    return;
  }

  const bool onGames = activeTab == Tab::Games;
  int& sel = onGames ? selectedIndex : appSelectedIndex;
  const auto& vis = onGames ? visibleGames : visibleApps;
  const int count = static_cast<int>(vis.size());

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left) {
    if (activeTab == Tab::Games) { activeTab = Tab::Apps; requestUpdate(); }
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right) {
    if (activeTab == Tab::Apps) { activeTab = Tab::Games; requestUpdate(); }
    else { finish(); }
    return;
  }

  if (count == 0) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
      if (activeTab == Tab::Apps) { activeTab = Tab::Games; requestUpdate(); }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) || mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
      if (activeTab == Tab::Games) { activeTab = Tab::Apps; requestUpdate(); }
    }
    int x = 0, y = 0;
    if (mappedInput.wasScreenTapped(x, y)) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int margin = std::max(16, metrics.contentSidePadding);
      const int w = renderer.getScreenWidth() - margin * 2;
      const int gap = 10;
      const int tabW = (w - gap) / 2;
      if (y >= tabBarTop() && y < tabBarTop() + tabBarHeight()) {
        if (x >= margin && x < margin + tabW) { activeTab = Tab::Games; requestUpdate(); }
        else if (x >= margin + tabW + gap && x < margin + tabW * 2 + gap) { activeTab = Tab::Apps; requestUpdate(); }
      } else {
        int bx, by, bw, bh;
        if (hiddenButtonRect(bx, by, bw, bh) && x >= bx && x < bx + bw && y >= by && y < by + bh) {
          openHiddenList();
        }
      }
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && hiddenCount(activeTab) > 0) {
      openHiddenList();
      return;
    }
    touchHoldIndex = -1;
    touchHoldFired = false;
    return;
  }

  // Long press for options (button / SDK events)
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, 700)) {
    showItemOptions(activeTab, sel);
    return;
  }
  int tx = 0, ty = 0;
  if (mappedInput.wasScreenLongPress(tx, ty)) {
    const int idx = indexAtPoint(tx, ty);
    if (idx >= 0) { sel = idx; showItemOptions(activeTab, idx); return; }
  }

  // Manual screen long-press fallback
  int hx = 0, hy = 0;
  if (mappedInput.isScreenTouchHeld(hx, hy)) {
    const int idx = indexAtPoint(hx, hy);
    const uint32_t now = millis();
    if (idx >= 0) {
      if (touchHoldIndex != idx || touchHoldTab != activeTab) {
        touchHoldIndex = idx;
        touchHoldTab = activeTab;
        touchHoldStartMs = now;
        touchHoldFired = false;
      } else if (!touchHoldFired && now - touchHoldStartMs >= 700) {
        touchHoldFired = true;
        sel = idx;
        showItemOptions(activeTab, idx);
        return;
      }
    } else {
      touchHoldIndex = -1;
      touchHoldFired = false;
    }
  } else {
    touchHoldIndex = -1;
    touchHoldFired = false;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) { sel = (sel + count - 1) % count; requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) { sel = (sel + 1) % count; requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (activeTab == Tab::Apps) { activeTab = Tab::Games; requestUpdate(); }
    else { sel = (sel + count - 1) % count; requestUpdate(); }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (activeTab == Tab::Games) { activeTab = Tab::Apps; requestUpdate(); }
    else { sel = (sel + 1) % count; requestUpdate(); }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (touchHoldFired) { touchHoldFired = false; return; }
    openSelectedIndex(sel);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) { activeTab = Tab::Games; requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) { activeTab = Tab::Apps; requestUpdate(); return; }

  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (touchHoldFired) { touchHoldFired = false; return; }
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int margin = std::max(16, metrics.contentSidePadding);
    const int w = renderer.getScreenWidth() - margin * 2;
    const int gap = 10;
    const int tabW = (w - gap) / 2;
    if (ty >= tabBarTop() && ty < tabBarTop() + tabBarHeight()) {
      if (tx >= margin && tx < margin + tabW) { activeTab = Tab::Games; requestUpdate(); return; }
      if (tx >= margin + tabW + gap && tx < margin + tabW * 2 + gap) { activeTab = Tab::Apps; requestUpdate(); return; }
    }
    int bx, by, bw, bh;
    if (hiddenButtonRect(bx, by, bw, bh) && tx >= bx && tx < bx + bw && ty >= by && ty < by + bh) {
      openHiddenList();
      return;
    }
    const int idx = indexAtPoint(tx, ty);
    if (idx >= 0) { sel = idx; openSelectedIndex(idx); }
  }
}

void GamesActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const char* header = showingHiddenList ? "Hidden items" : tr(STR_GAMES);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, header);
  drawTabs();

  const int rh = rowHeight();
  const int top = listTop();
  const int margin = std::max(16, metrics.contentSidePadding);
  const bool onGames = activeTab == Tab::Games;

  if (showingHiddenList) {
    const auto& hv = onGames ? hiddenGames : hiddenApps;
    const int count = static_cast<int>(hv.size());
    const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
    if (count == 0) {
      renderer.drawCenteredText(UI_12_FONT_ID, top + 20, "No hidden items");
    } else {
      for (int i = 0; i < count; ++i) {
        const int y = top + i * rh;
        if (y + rh > bottom) break;
        const bool selected = i == hiddenListIndex;
        const int rowX = margin;
        const int rowW = renderer.getScreenWidth() - margin * 2;
        const int boxH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
        if (selected) {
          renderer.drawRect(rowX, y, rowW, boxH, 2, true);
          renderer.fillRect(rowX + 3, y + 3, 4, boxH - 6);
        } else {
          renderer.drawRect(rowX, y, rowW, boxH);
        }
        std::string name = hv[i];
        if (onGames) {
          if (name == "chess") name = "Chess";
          else if (name == "connectfour") name = tr(STR_CONNECT_FOUR);
          else if (name == "minesweeper") name = tr(STR_MINESWEEPER);
          else if (name == "nonograms") name = tr(STR_NONOGRAMS);
          else if (name == "snake") name = tr(STR_SNAKE);
          else if (name == "sudoku") name = tr(STR_SUDOKU);
        } else {
          if (name == "calculator") name = "Calculator";
          else if (name == "stopwatch") name = "Stopwatch";
          else if (name == "notes") name = "Notes";
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "Unhide: %s", name.c_str());
        const int textY = y + (boxH - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        renderer.drawText(UI_12_FONT_ID, rowX + 16, textY, buf, true);
      }
    }
  } else {
    const auto& vis = onGames ? visibleGames : visibleApps;
    const int sel = onGames ? selectedIndex : appSelectedIndex;
    const int count = static_cast<int>(vis.size());
    const int bottom = mainListBottom();
    if (count == 0) {
      renderer.drawCenteredText(UI_12_FONT_ID, top + 20, "No items");
    } else {
      for (int i = 0; i < count; ++i) {
        const int y = top + i * rh;
        if (y + rh > bottom) break;
        const bool selected = i == sel;
        const int rowX = margin;
        const int rowW = renderer.getScreenWidth() - margin * 2;
        const int boxH = std::max(44, renderer.getLineHeight(UI_12_FONT_ID) + 20);
        if (selected) {
          renderer.drawRect(rowX, y, rowW, boxH, 2, true);
          renderer.fillRect(rowX + 3, y + 3, 4, boxH - 6);
        } else {
          renderer.drawRect(rowX, y, rowW, boxH);
        }

        const Item& item = onGames ? kGames[vis[i]] : kApps[vis[i]];
        const bool pinned = isPinned(activeTab, item.id);
        int textX = rowX + 16;
        if (item.icon) {
          const int iconSize = 32;
          const int iconX = rowX + 14;
          const int iconY = y + (boxH - iconSize) / 2;
          renderer.drawIcon(item.icon, iconX, iconY, iconSize);
          textX = iconX + iconSize + 12;
        }
        const char* name = item.label;
        if (onGames) {
          if (std::string(item.id) == "chess") name = "Chess";
          else if (std::string(item.id) == "connectfour") name = tr(STR_CONNECT_FOUR);
          else if (std::string(item.id) == "minesweeper") name = tr(STR_MINESWEEPER);
          else if (std::string(item.id) == "nonograms") name = tr(STR_NONOGRAMS);
          else if (std::string(item.id) == "snake") name = tr(STR_SNAKE);
          else if (std::string(item.id) == "sudoku") name = tr(STR_SUDOKU);
        } else {
          if (std::string(item.id) == "calculator") name = "Calculator";
          else if (std::string(item.id) == "stopwatch") name = "Stopwatch";
          else name = "Notes";
        }
        char buf[64];
        if (pinned) snprintf(buf, sizeof(buf), "* %s", name); else snprintf(buf, sizeof(buf), "%s", name);
        const int textY = y + (boxH - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        renderer.drawText(UI_12_FONT_ID, textX, textY, buf, true);
      }
    }

    int bx, by, bw, bh;
    if (hiddenButtonRect(bx, by, bw, bh)) {
      renderer.drawRect(bx, by, bw, bh, true);
      char hbuf[48];
      snprintf(hbuf, sizeof(hbuf), "Manage hidden (%d)", hiddenCount(activeTab));
      const int tw = renderer.getTextWidth(UI_10_FONT_ID, hbuf);
      const int tx = bx + (bw - tw) / 2;
      const int ty = by + (bh - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
      renderer.drawText(UI_10_FONT_ID, tx, ty, hbuf, true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showingHiddenList ? "Unhide" : tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
