#pragma once

#include <array>
#include <cstdint>
#include "activities/Activity.h"

class StopwatchActivity final : public Activity {
 public:
  StopwatchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Action : uint8_t { StartStop = 0, LapReset, Count };
  void toggleRun();
  void lapOrReset();
  void formatTime(char* out, size_t outSize, uint32_t ms) const;

  bool running = false;
  uint32_t elapsedMs = 0;
  uint32_t lastTickMs = 0;
  std::array<uint32_t, 8> laps{};
  int lapCount = 0;
  int selectedAction = 0;
  bool needsRedraw = true;
};
