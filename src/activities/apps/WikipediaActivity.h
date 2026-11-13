#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

class WikipediaActivity final : public Activity {
 public:
  WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t {
    IdleMenu,
    EnterQuery,
    Searching,
    Results,
    SelectDownloadLang,
    Settings,
    Fetching,
    Error,
    Opening,
  };

 public:
  struct LangOption {
    const char* code;
    const char* name;
  };

 private:

  struct PendingDownload {
    int resultIndex = -1;
    std::string sourceLang;
    std::string sourceTitle;
    std::vector<std::string> langCodes;
    std::vector<std::string> langTitles;
  };

  void loadSettings();
  void saveSettings() const;
  void ensureDefaultLanguages();
  bool isLanguageSelected(const std::string& code) const;
  void toggleLanguage(const std::string& code);

  void showMenu();
  void promptSearch();
  void startSearch(const std::string& query);
  void onResultSelected(int index);
  void fetchLangLinksAndPrompt(int index);
  void fetchArticle(const std::string& lang, const std::string& title);
  void showError(const std::string& message);
  std::string urlEncodeComponent(const std::string& s) const;
  std::string sanitizeFilename(const std::string& title) const;
  const char* languageName(const std::string& code) const;

  State state = State::IdleMenu;
  std::string statusLine;
  std::string errorMessage;
  std::string currentQuery;
  std::string searchLang = "en";
  std::vector<std::string> resultTitles;
  std::vector<std::string> resultDescs;
  int selectedResult = 0;
  int menuIndex = 0;
  int settingsIndex = 0;
  int langSelectIndex = 0;
  PendingDownload pendingDownload;
  std::vector<std::string> selectedLanguages;  // enabled Wikipedia language codes
  bool needsRedraw = true;
  bool wifiSelectionPending = false;
  bool returnToMenuOnBack = true;
};
