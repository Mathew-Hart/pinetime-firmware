#include "components/heartrate/HeartRateController.h"
#include <heartratetask/HeartRateTask.h>
#include <task.h>

using namespace Pinetime::Controllers;

void HeartRateController::Update(HeartRateController::States newState, uint8_t value) {
  state.store(newState);
  if (heartRate.exchange(value) != value && service != nullptr) {
    service->OnNewHeartRateValue(value);
  }
}

void HeartRateController::Enable() {
  if (auto* target = task.load(); target != nullptr) {
    state.store(States::NotEnoughData);
    target->PushMessage(Pinetime::Applications::HeartRateTask::Messages::Enable);
  }
}

void HeartRateController::Disable() {
  if (auto* target = task.load(); target != nullptr) {
    state.store(States::Stopped);
    target->PushMessage(Pinetime::Applications::HeartRateTask::Messages::Disable);
  }
}

void HeartRateController::SetAnalogFaceVisible(bool visible) {
  if (!visible) {
    analogFaceCancellation.store(true);
  }
  if (analogFaceVisible.exchange(visible) != visible) {
    if (auto* target = task.load(); target != nullptr) {
      target->PushMessage(Pinetime::Applications::HeartRateTask::Messages::AnalogVisibilityChanged);
    }
  }
}

HeartRateController::AnalogFaceSample HeartRateController::GetAnalogFaceSample() const {
  const auto before = analogFaceSampleVersion.load();
  const auto value = analogFaceSample.load();
  const auto sampleTime = analogFaceSampleTime.load();
  const auto after = analogFaceSampleVersion.load();
  // Do not spin: the display can preempt the publisher. A transient unavailable
  // result is safe; waiting for an interrupted lower-priority writer is not.
  if ((before & 1U) != 0 || before != after) {
    return {0, false, false};
  }
  const bool fresh = xTaskGetTickCount() - sampleTime < 600U * configTICK_RATE_HZ;
  const bool valid = (value & 0x100) != 0 && fresh;
  return {valid ? static_cast<uint8_t>(value & 0xff) : static_cast<uint8_t>(0), valid, (value & 0x200) != 0};
}

void HeartRateController::PublishAnalogFaceSample(uint8_t bpm, bool valid, bool measuring) {
  const bool usable = valid && bpm != 0;
  analogFaceSampleVersion.fetch_add(1);
  if (usable) {
    analogFaceSampleTime.store(xTaskGetTickCount());
  }
  analogFaceSample.store((usable ? bpm : 0) | (usable ? 0x100U : 0) | (measuring ? 0x200U : 0));
  analogFaceSampleVersion.fetch_add(1);
}

void HeartRateController::SetHeartRateTask(Pinetime::Applications::HeartRateTask* value) {
  task.store(value);
}

void HeartRateController::SetService(Pinetime::Controllers::HeartRateService* value) {
  service = value;
}
