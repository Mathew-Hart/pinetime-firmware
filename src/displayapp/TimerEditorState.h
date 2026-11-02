#pragma once

#include <cstdint>

namespace Pinetime::Applications {
  // The shared timer controller owns running/expired timers. Keep only the
  // editor's unstarted or paused input while navigating between clock tools.
  struct TimerEditorState {
    uint16_t seconds = 0;
  };
}
