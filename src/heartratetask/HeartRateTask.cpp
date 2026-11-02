#include "heartratetask/HeartRateTask.h"
#include <drivers/Hrs3300.h>
#include <components/heartrate/HeartRateController.h>
#include <limits>

#include "utility/Math.h"

using namespace Pinetime::Applications;

namespace {
  constexpr TickType_t backgroundMeasurementTimeLimit = 30 * configTICK_RATE_HZ;
}

std::optional<TickType_t> HeartRateTask::BackgroundMeasurementInterval() const {
  auto interval = settings.GetHeartRateBackgroundMeasurementInterval();
  if (!interval.has_value()) {
    return std::nullopt;
  }
  return interval.value() * configTICK_RATE_HZ;
}

bool HeartRateTask::BackgroundMeasurementNeeded() const {
  auto backgroundPeriod = BackgroundMeasurementInterval();
  if (!backgroundPeriod.has_value()) {
    return false;
  }
  return xTaskGetTickCount() - lastMeasurementTime >= backgroundPeriod.value();
};

TickType_t HeartRateTask::CurrentTaskDelay() {
  auto backgroundPeriod = BackgroundMeasurementInterval();
  TickType_t currentTime = xTaskGetTickCount();
  auto CalculateSleepTicks = [&]() {
    TickType_t elapsed = currentTime - measurementStartTime;

    // Target system tick is the elapsed sensor ticks multiplied by the sensor tick duration (i.e. the elapsed time)
    // multiplied by the system tick rate
    // Since the sensor tick duration is a whole number of milliseconds, we compute in milliseconds and then divide by 1000
    // To avoid the number of milliseconds overflowing a u32, we take a factor of 2 out of the divisor and dividend
    // (1024 / 2) * 65536 * 100 = 3355443200 which is less than 2^32

    // Guard against future tick rate changes
    static_assert((configTICK_RATE_HZ / 2ULL) * (std::numeric_limits<decltype(count)>::max() + 1ULL) *
                      static_cast<uint64_t>((Pinetime::Controllers::Ppg::deltaTms)) <
                    std::numeric_limits<uint32_t>::max(),
                  "Overflow");
    TickType_t elapsedTarget = Utility::RoundedDiv(static_cast<uint32_t>(configTICK_RATE_HZ / 2) * (static_cast<uint32_t>(count) + 1U) *
                                                     static_cast<uint32_t>((Pinetime::Controllers::Ppg::deltaTms)),
                                                   static_cast<uint32_t>(1000 / 2));

    // On count overflow, reset both count and start time
    // Count is 16bit to avoid overflow in elapsedTarget
    // Count overflows every 100ms * u16 max = ~2 hours, much more often than the tick count (~48 days)
    // So no need to check for tick count overflow
    if (count == std::numeric_limits<decltype(count)>::max()) {
      count = 0;
      measurementStartTime = currentTime;
    }
    if (elapsedTarget > elapsed) {
      return elapsedTarget - elapsed;
    }
    return static_cast<TickType_t>(0);
  };
  TickType_t delay = portMAX_DELAY;
  if (sensorRunning) {
    delay = CalculateSleepTicks();
  } else if ((LegacyMeasuring() || analogSchedule.Active()) && sensorStartFailed) {
    const auto elapsed = currentTime - lastSensorStartAttempt;
    delay = elapsed >= configTICK_RATE_HZ ? 0 : configTICK_RATE_HZ - elapsed;
  } else if (state == States::Waiting && backgroundPeriod.has_value()) {
    const auto elapsed = currentTime - lastMeasurementTime;
    delay = elapsed >= backgroundPeriod.value() ? 0 : backgroundPeriod.value() - elapsed;
  }
  if (auto analogDelay = analogSchedule.Delay(currentTime); analogDelay.has_value() && analogDelay.value() < delay) {
    delay = analogDelay.value();
  }
  return delay;
}

HeartRateTask::HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings)
  : heartRateSensor {heartRateSensor}, controller {controller}, settings {settings} {
}

void HeartRateTask::Start() {
  messageQueue = xQueueCreate(10, 1);
  controller.SetHeartRateTask(this);

  if (pdPASS != xTaskCreate(HeartRateTask::Process, "Heartrate", 500, this, 1, &taskHandle)) {
    APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
  }
}

void HeartRateTask::Process(void* instance) {
  auto* app = static_cast<HeartRateTask*>(instance);
  app->Work();
}

void HeartRateTask::Work() {
  // measurementStartTime is always initialised before use by StartMeasurement
  // Need to initialise lastMeasurementTime so that the first background measurement happens at a reasonable time
  lastMeasurementTime = xTaskGetTickCount();
  valueCurrentlyShown = false;

  // First evaluate retained controller visibility before sleeping. The display
  // task starts before this task and can request the analog face during boot.
  TickType_t delay = 0;
  while (true) {
    Messages msg;
    States newState = state;

    if (xQueueReceive(messageQueue, &msg, delay) == pdTRUE) {
      switch (msg) {
        case Messages::GoToSleep:
          if (state != States::Disabled) {
            newState = BackgroundMeasurementNeeded() ? States::BackgroundMeasuring : States::Waiting;
          }
          break;
        case Messages::WakeUp:
          // Wake affects only the existing manual/background owner. An analog
          // one-shot must never become an unbounded foreground measurement.
          if (state != States::Disabled) {
            newState = States::ForegroundMeasuring;
          }
          break;
        case Messages::Enable:
          newState = States::ForegroundMeasuring;
          valueCurrentlyShown = false;
          break;
        case Messages::Disable:
          newState = States::Disabled;
          break;
        case Messages::AnalogVisibilityChanged:
          break; // Read authoritative desired state below, not a stale event.
      }
    }
    if (newState == States::Waiting && BackgroundMeasurementNeeded()) {
      newState = States::BackgroundMeasuring;
    } else if (newState == States::BackgroundMeasuring && !BackgroundMeasurementNeeded()) {
      newState = States::Waiting;
    }
    if (newState == States::BackgroundMeasuring && state != States::BackgroundMeasuring) {
      backgroundStartAttemptTime = xTaskGetTickCount();
    }
    // Bound the whole positive-interval background request, including failed
    // starts and failed reads that never reach the PPG completion path. Keeping
    // this check outside data handling makes all early returns obey the budget.
    // The explicit continuous (zero interval) preference retains its behavior.
    const auto backgroundPeriod = BackgroundMeasurementInterval();
    if (newState == States::BackgroundMeasuring && backgroundPeriod.has_value() && backgroundPeriod.value() != 0 &&
        xTaskGetTickCount() - backgroundStartAttemptTime >= backgroundMeasurementTimeLimit) {
      lastMeasurementTime = backgroundPeriod.value() > backgroundMeasurementTimeLimit ? backgroundStartAttemptTime : xTaskGetTickCount();
      newState = States::Waiting;
      controller.Update(Controllers::HeartRateController::States::Running, 0);
    }
    state = newState;

    UpdateAnalogState();

    UpdateSensorState();
    // Enabling the physical sensor yields briefly. A face can disappear while
    // this task is suspended; cancellation wins before any acquisition.
    UpdateAnalogState();
    UpdateSensorState();
    if (sensorRunning) {
      HandleSensorData();
      count++;
      // The first valid sample ends the analog request immediately. A manual
      // or background owner keeps the shared sensor running independently.
      UpdateSensorState();
    }
    delay = CurrentTaskDelay();
  }
}

void HeartRateTask::UpdateAnalogState() {
  // A hide/show burst may occur before this task gets CPU time. Remember the
  // hide rather than treating the final true value as an uninterrupted capture.
  if (controller.TakeAnalogFaceCancellation() &&
      analogSchedule.Update(false, xTaskGetTickCount()) == Controllers::AnalogHeartRateSchedule::Event::Canceled) {
    controller.PublishAnalogFaceSample(0, false, false);
  }
  switch (analogSchedule.Update(controller.AnalogFaceVisible(), xTaskGetTickCount())) {
    case Controllers::AnalogHeartRateSchedule::Event::Started:
      controller.PublishAnalogFaceSample(0, false, true);
      break;
    case Controllers::AnalogHeartRateSchedule::Event::Canceled:
    case Controllers::AnalogHeartRateSchedule::Event::TimedOut:
      controller.PublishAnalogFaceSample(0, false, false);
      break;
    case Controllers::AnalogHeartRateSchedule::Event::None:
      break;
  }
}

bool HeartRateTask::LegacyMeasuring() const {
  return state == States::ForegroundMeasuring || state == States::BackgroundMeasuring;
}

void HeartRateTask::UpdateSensorState() {
  const bool needed = LegacyMeasuring() || analogSchedule.Active();
  if (needed && !sensorRunning && (!sensorStartFailed || xTaskGetTickCount() - lastSensorStartAttempt >= configTICK_RATE_HZ)) {
    lastSensorStartAttempt = xTaskGetTickCount();
    sensorRunning = StartMeasurement();
    sensorStartFailed = !sensorRunning;
  } else if (!needed && sensorRunning) {
    StopMeasurement();
    sensorRunning = false;
  }
}

void HeartRateTask::PushMessage(HeartRateTask::Messages msg) {
  if (messageQueue == nullptr) {
    return;
  }
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(messageQueue, &msg, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

bool HeartRateTask::StartMeasurement() {
  ppg.Reset(true);
  if (!heartRateSensor.Enable()) {
    if (analogSchedule.Active()) {
      analogSchedule.Complete();
      controller.PublishAnalogFaceSample(0, false, false);
    }
    if (LegacyMeasuring()) {
      controller.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
    }
    return false;
  }
  vTaskDelay(100);
  measurementSucceeded = false;
  count = 0;
  measurementStartTime = xTaskGetTickCount();
  return true;
}

void HeartRateTask::StopMeasurement() {
  heartRateSensor.Disable();
  ppg.Reset(true);
  vTaskDelay(100);
}

void HeartRateTask::HandleSensorData() {
  const auto sensorData = heartRateSensor.ReadHrsAls();
  if (!sensorData.has_value()) {
    // Do not feed incomplete I2C data (or the preceding PPG window) into a new
    // sample. An analog attempt fails closed and can retry at its next period.
    ppg.Reset(true);
    if (analogSchedule.Active()) {
      analogSchedule.Complete();
      controller.PublishAnalogFaceSample(0, false, false);
    }
    if (LegacyMeasuring()) {
      controller.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
      valueCurrentlyShown = false;
    }
    return;
  }
  // I2C may also yield. Never publish a face sample after it became hidden or
  // exceeded its limit while the transaction was in flight.
  UpdateAnalogState();
  if (!LegacyMeasuring() && !analogSchedule.Active()) {
    return;
  }
  const int8_t ambient = ppg.Preprocess(sensorData->hrs, sensorData->als);
  int bpm = ppg.HeartRate();

  if (ambient > 0) {
    ppg.Reset(true);
    bpm = 0;
    valueCurrentlyShown = false;
    if (LegacyMeasuring()) {
      controller.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
    }
  }

  if (bpm == -1) {
    ppg.Reset(false);
    bpm = 0;
    valueCurrentlyShown = false;
    if (LegacyMeasuring()) {
      controller.Update(Controllers::HeartRateController::States::Running, 0);
    }
  } else if (bpm == -2) {
    bpm = 0;
    if (LegacyMeasuring() && !valueCurrentlyShown) {
      controller.Update(Controllers::HeartRateController::States::NotEnoughData, 0);
    }
  }

  if (bpm > 0 && bpm <= std::numeric_limits<uint8_t>::max()) {
    UpdateAnalogState();
    if (analogSchedule.Active()) {
      // Completion belongs to this attempt, even when the BPM equals the last
      // sample. Never infer freshness from the global BLE value changing.
      analogSchedule.Complete();
      controller.PublishAnalogFaceSample(static_cast<uint8_t>(bpm), true, false);
    }
    if (LegacyMeasuring()) {
      if (state == States::BackgroundMeasuring && xTaskGetTickCount() - measurementStartTime < backgroundMeasurementTimeLimit) {
        lastMeasurementTime = measurementStartTime;
      } else {
        lastMeasurementTime = xTaskGetTickCount();
      }
      measurementSucceeded = true;
      valueCurrentlyShown = true;
      controller.Update(Controllers::HeartRateController::States::Running, static_cast<uint8_t>(bpm));
    }
    return;
  }

  // Preserve the existing continuous/background measurement policy. It is
  // independent of the bounded visible-face request above.
  if (LegacyMeasuring() && xTaskGetTickCount() - measurementStartTime > backgroundMeasurementTimeLimit) {
    if (!measurementSucceeded) {
      controller.Update(Controllers::HeartRateController::States::Running, 0);
      valueCurrentlyShown = false;
    }
    if (state == States::BackgroundMeasuring) {
      lastMeasurementTime = xTaskGetTickCount() - backgroundMeasurementTimeLimit;
    } else {
      lastMeasurementTime = xTaskGetTickCount();
    }
  }
}
