#pragma once

#include <array>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class GamesActivity final : public Activity {
 public:
  GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Tab : uint8_t { Games = 0, Apps, Count };
  struct Item { const char* id; const char* label; const uint8_t* icon; };

  void openSelected();
  void openSelectedIndex(int idx);
  void rebuildLists();
  void loadPrefs();
  void savePrefs() const;
  bool isPinned(Tab tab, const char* id) const;
  bool isHidden(Tab tab, const char* id) const;
  void setPinned(Tab tab, const char* id, bool pinned);
  void setHidden(Tab tab, const char* id, bool hidden);
  void showItemOptions(Tab tab, int visibleIndex);
  void showHideConfirm(Tab tab, const char* id, const char* label);

  int tabBarTop() const;
  int tabBarHeight() const;
  int listTop() const;
  int rowHeight() const;
  void drawTabs() const;
  int indexAtPoint(int x, int y) const;

  Tab activeTab = Tab::Games;
  int selectedIndex = 0;
  int appSelectedIndex = 0;
  OptionPopup optionPopup;

  std::vector<std::string> pinnedGames;
  std::vector<std::string> pinnedApps;
  std::vector<std::string> hiddenGames;
  std::vector<std::string> hiddenApps;

  std::vector<int> visibleGames; // indices into kGames
  std::vector<int> visibleApps;

  static const Item kGames[];
  static const Item kApps[];
  static constexpr int kGameCount = 6;
  static constexpr int kAppCount = 2;
};
