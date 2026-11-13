#pragma once

#include "activities/Activity.h"

class GamesActivity final : public Activity {
 public:
  GamesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void openSudoku();
  int tileLeft() const;
  int tileTop() const;
  int tileWidth() const;
  int tileHeight() const;
};
