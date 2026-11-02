#pragma once
#include <FreeRTOS.h>
#include <cstdint>
#include <optional>
#include <task.h>
#include <queue.h>
#include <components/heartrate/Ppg.h>
#include <components/heartrate/AnalogHeartRateSchedule.h>
#include "components/settings/Settings.h"

namespace Pinetime {
  namespace Drivers {
    class Hrs3300;
  }

  namespace Controllers {
    class HeartRateController;
  }

  namespace Applications {
    class HeartRateTask {
    public:
      enum class Messages : uint8_t { GoToSleep, WakeUp, Enable, Disable, AnalogVisibilityChanged };

      explicit HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings);
      void Start();
      void Work();
      void PushMessage(Messages msg);

    private:
      enum class States : uint8_t { Disabled, Waiting, BackgroundMeasuring, ForegroundMeasuring };
      static void Process(void* instance);
      void HandleSensorData();
      bool StartMeasurement();
      void StopMeasurement();
      [[nodiscard]] bool LegacyMeasuring() const;
      void UpdateSensorState();
      void UpdateAnalogState();

      [[nodiscard]] bool BackgroundMeasurementNeeded() const;
      [[nodiscard]] std::optional<TickType_t> BackgroundMeasurementInterval() const;
      TickType_t CurrentTaskDelay();

      TaskHandle_t taskHandle = nullptr;
      QueueHandle_t messageQueue = nullptr;
      bool valueCurrentlyShown = false;
      bool measurementSucceeded = false;
      States state = States::Disabled;
      uint16_t count = 0;
      Drivers::Hrs3300& heartRateSensor;
      Controllers::HeartRateController& controller;
      Controllers::Settings& settings;
      Controllers::Ppg ppg;
      bool sensorRunning = false;
      bool sensorStartFailed = false;
      TickType_t lastSensorStartAttempt = 0;
      TickType_t backgroundStartAttemptTime = 0;
      Controllers::AnalogHeartRateSchedule analogSchedule {configTICK_RATE_HZ};
      TickType_t lastMeasurementTime = 0;
      TickType_t measurementStartTime = 0;
    };

  }
}
