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
  enum class State : uint8_t { Idle, EnterQuery, Searching, Results, Fetching, Error, Opening };

  void promptSearch();
  void startSearch(const std::string& query);
  void fetchArticle(int index);
  void showError(const std::string& message);
  std::string urlEncodeComponent(const std::string& s) const;
  std::string sanitizeFilename(const std::string& title) const;

  State state = State::Idle;
  std::string statusLine;
  std::string errorMessage;
  std::string currentQuery;
  std::vector<std::string> resultTitles;
  std::vector<std::string> resultDescs;
  int selectedResult = 0;
  bool needsRedraw = true;
  bool wifiSelectionPending = false;
};
