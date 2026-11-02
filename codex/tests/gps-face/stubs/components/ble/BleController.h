#pragma once
namespace Pinetime::Controllers {
  class Ble {
  public:
    bool IsConnected() const { return connected; }
    bool connected = true;
  };
}
