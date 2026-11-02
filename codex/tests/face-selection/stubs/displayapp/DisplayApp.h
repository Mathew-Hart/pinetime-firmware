#pragma once
namespace Pinetime::Applications {
  class DisplayApp {
  public:
    enum class FullRefreshDirections { None, Up, Down, LeftAnim, RightAnim };
    void SetFullRefresh(FullRefreshDirections value) { direction = value; ++refreshes; }
    FullRefreshDirections direction = FullRefreshDirections::None;
    unsigned refreshes = 0;
  };
}
