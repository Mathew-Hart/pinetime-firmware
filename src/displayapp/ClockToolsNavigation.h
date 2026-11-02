#pragma once

#include "displayapp/apps/Apps.h"

namespace Pinetime::Applications {
  constexpr bool IsClockTool(Apps app) {
    return app == Apps::Alarm || app == Apps::Timer || app == Apps::StopWatch;
  }

  enum class ClockToolsRoute { Normal, ReplaceTool, InsertChooser };

  // Each tool has the chooser as its immediate back target. Alerting from another
  // app also inserts the chooser, while switching between tools replaces the
  // existing child instead of accumulating a chain of tool screens.
  constexpr ClockToolsRoute RouteClockTool(Apps current, Apps next) {
    if (!IsClockTool(next) || current == Apps::ClockTools) {
      return ClockToolsRoute::Normal;
    }
    return IsClockTool(current) ? ClockToolsRoute::ReplaceTool : ClockToolsRoute::InsertChooser;
  }
}
