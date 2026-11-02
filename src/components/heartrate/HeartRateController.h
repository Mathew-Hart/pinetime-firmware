#pragma once

#include <atomic>
#include <cstdint>
#include <components/ble/HeartRateService.h>

namespace Pinetime {
  namespace Applications {
    class HeartRateTask;
  }

  namespace Controllers {
    class HeartRateController {
    public:
      enum class States : uint8_t { Stopped, NotEnoughData, NoTouch, Running };
      struct AnalogFaceSample {
        uint8_t heartRate;
        bool valid;
        bool measuring;
      };

      HeartRateController() = default;
      void Enable();
      void Disable();
      void Update(States newState, uint8_t heartRate);
      void SetHeartRateTask(Applications::HeartRateTask* task);

      States State() const {
        return state.load();
      }

      uint8_t HeartRate() const {
        return heartRate.load();
      }

      // Visibility is desired state, not a queued command: a full task queue or
      // an early display startup cannot lose a cancellation/request.
      void SetAnalogFaceVisible(bool visible);
      bool AnalogFaceVisible() const {
        return analogFaceVisible.load();
      }
      bool TakeAnalogFaceCancellation() {
        return analogFaceCancellation.exchange(false);
      }
      AnalogFaceSample GetAnalogFaceSample() const;
      void PublishAnalogFaceSample(uint8_t bpm, bool valid, bool measuring);

      void SetService(Pinetime::Controllers::HeartRateService* service);

    private:
      std::atomic<Applications::HeartRateTask*> task {nullptr};
      std::atomic<States> state {States::Stopped};
      std::atomic<uint8_t> heartRate {0};
      std::atomic<bool> analogFaceVisible {false};
      std::atomic<bool> analogFaceCancellation {false};
      // BPM + validity + measurement flag are published together. In
      // particular, a new attempt never exposes the preceding attempt's BPM.
      std::atomic<uint32_t> analogFaceSample {0};
      std::atomic<uint32_t> analogFaceSampleTime {0};
      std::atomic<uint32_t> analogFaceSampleVersion {0};
      Pinetime::Controllers::HeartRateService* service = nullptr;
    };
  }
}
