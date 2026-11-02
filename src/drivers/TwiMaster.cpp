#include "drivers/TwiMaster.h"
#include <cstring>
#include <hal/nrf_gpio.h>
#include <nrfx_log.h>
#include <task.h>

using namespace Pinetime::Drivers;

// TODO use shortcut to automatically send STOP when receive LastTX, for example
// TODO use DMA/IRQ

TwiMaster::TwiMaster(NRF_TWIM_Type* module, uint32_t frequency, uint8_t pinSda, uint8_t pinScl)
  : module {module}, frequency {frequency}, pinSda {pinSda}, pinScl {pinScl} {
}

void TwiMaster::ConfigurePins() const {
  NRF_GPIO->PIN_CNF[pinScl] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                              (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
                              (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  NRF_GPIO->PIN_CNF[pinSda] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                              (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
                              (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void TwiMaster::Init() {
  if (mutex == nullptr) {
    mutex = xSemaphoreCreateBinary();
  }

  ConfigurePins();

  twiBaseAddress = module;

  twiBaseAddress->FREQUENCY = frequency;

  twiBaseAddress->PSEL.SCL = pinScl;
  twiBaseAddress->PSEL.SDA = pinSda;
  twiBaseAddress->EVENTS_LASTRX = 0;
  twiBaseAddress->EVENTS_STOPPED = 0;
  twiBaseAddress->EVENTS_LASTTX = 0;
  twiBaseAddress->EVENTS_ERROR = 0;
  twiBaseAddress->EVENTS_RXSTARTED = 0;
  twiBaseAddress->EVENTS_SUSPENDED = 0;
  twiBaseAddress->EVENTS_TXSTARTED = 0;

  twiBaseAddress->ENABLE = (TWIM_ENABLE_ENABLE_Enabled << TWIM_ENABLE_ENABLE_Pos);

  xSemaphoreGive(mutex);
}

TwiMaster::ErrorCodes TwiMaster::Read(uint8_t deviceAddress, uint8_t registerAddress, uint8_t* data, size_t size) {
  return ReadDiagnostic(deviceAddress, registerAddress, data, size).IsOk() ? ErrorCodes::NoError : ErrorCodes::TransactionFailed;
}

TwiMaster::DiagnosticResult TwiMaster::ReadDiagnostic(uint8_t deviceAddress, uint8_t registerAddress, uint8_t* data, size_t size) {
  if (deviceAddress > 0x7f || data == nullptr || size == 0 || size > maxDataSize) {
    return {DiagnosticError::InvalidArgument, 0};
  }
  if (mutex == nullptr || xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return {DiagnosticError::LockTimeout, 0};
  }
  Wakeup();
  ClearEvents();
  auto result = Write(deviceAddress, &registerAddress, 1, false);
  // A failed register-pointer write must never be overwritten by an RX result.
  if (result.IsOk()) {
    result = Read(deviceAddress, data, size, true);
  }
  Sleep();
  xSemaphoreGive(mutex);
  return result;
}

TwiMaster::ErrorCodes TwiMaster::Write(uint8_t deviceAddress, uint8_t registerAddress, const uint8_t* data, size_t size) {
  if (deviceAddress > 0x7f || size > maxDataSize || (size > 0 && data == nullptr) || mutex == nullptr ||
      xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return ErrorCodes::TransactionFailed;
  }
  Wakeup();
  ClearEvents();
  internalBuffer[0] = registerAddress;
  if (size > 0) {
    std::memcpy(internalBuffer + 1, data, size);
  }
  const auto result = Write(deviceAddress, internalBuffer, size + 1, true);
  Sleep();
  xSemaphoreGive(mutex);
  return result.IsOk() ? ErrorCodes::NoError : ErrorCodes::TransactionFailed;
}

void TwiMaster::ClearEvents() {
  twiBaseAddress->EVENTS_LASTRX = 0;
  twiBaseAddress->EVENTS_STOPPED = 0;
  twiBaseAddress->EVENTS_LASTTX = 0;
  twiBaseAddress->EVENTS_ERROR = 0;
  twiBaseAddress->EVENTS_RXSTARTED = 0;
  twiBaseAddress->EVENTS_SUSPENDED = 0;
  twiBaseAddress->EVENTS_TXSTARTED = 0;
  // ERRORSRC is write-one-to-clear on this peripheral.
  twiBaseAddress->ERRORSRC = twiBaseAddress->ERRORSRC;
}

TwiMaster::DiagnosticResult TwiMaster::BusError() const {
  const uint32_t source = twiBaseAddress->ERRORSRC;
  if ((source & (1UL << 1)) != 0) {
    return {DiagnosticError::AddressNack, source};
  }
  if ((source & (1UL << 2)) != 0) {
    return {DiagnosticError::DataNack, source};
  }
  if ((source & (1UL << 0)) != 0) {
    return {DiagnosticError::Overrun, source};
  }
  return {DiagnosticError::UnknownBusError, source};
}

TwiMaster::DiagnosticResult TwiMaster::WaitForEvent(volatile uint32_t& event, DiagnosticError timeout) {
  const uint32_t started = DWT->CYCCNT;
  const TickType_t startedTick = xTaskGetTickCount();
  const TickType_t timeoutTicks = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
  for (uint32_t polls = 0; polls < MaxPolls; ++polls) {
    if (twiBaseAddress->EVENTS_ERROR != 0) {
      return BusError();
    }
    if (event != 0) {
      event = 0;
      return {DiagnosticError::None, 0};
    }
    if (static_cast<uint32_t>(DWT->CYCCNT - started) > HwFreezedDelay ||
        static_cast<TickType_t>(xTaskGetTickCount() - startedTick) >= timeoutTicks) {
      break;
    }
  }
  return {timeout, 0};
}

TwiMaster::DiagnosticResult TwiMaster::AbortTransfer(DiagnosticResult result) {
  // Clear the captured error before waiting for STOP, but preserve it in result.
  twiBaseAddress->EVENTS_ERROR = 0;
  twiBaseAddress->ERRORSRC = twiBaseAddress->ERRORSRC;
  // STOP cannot complete while TWIM is suspended; Nordic's driver resumes
  // before stopping on errors for the same reason.
  twiBaseAddress->TASKS_RESUME = 1;
  twiBaseAddress->TASKS_STOP = 1;
  const auto stopped = WaitForEvent(twiBaseAddress->EVENTS_STOPPED, DiagnosticError::StopTimeout);
  if (!stopped.IsOk()) {
    // No sensor reset/configuration is issued: recover only the MCU peripheral.
    FixHwFreezed();
  }
  ClearEvents();
  return result;
}

TwiMaster::DiagnosticResult TwiMaster::FinishTransfer(bool stop) {
  if (stop) {
    twiBaseAddress->TASKS_STOP = 1;
  } else {
    twiBaseAddress->TASKS_SUSPEND = 1;
  }
  const auto result = stop ? WaitForEvent(twiBaseAddress->EVENTS_STOPPED, DiagnosticError::StopTimeout)
                           : WaitForEvent(twiBaseAddress->EVENTS_SUSPENDED, DiagnosticError::SuspendTimeout);
  return result.IsOk() ? result : AbortTransfer(result);
}

TwiMaster::DiagnosticResult TwiMaster::Read(uint8_t deviceAddress, uint8_t* buffer, size_t size, bool stop) {
  twiBaseAddress->ADDRESS = deviceAddress;
  twiBaseAddress->RXD.PTR = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer));
  twiBaseAddress->RXD.MAXCNT = size;
  twiBaseAddress->TASKS_STARTRX = 1;
  twiBaseAddress->TASKS_RESUME = 1;
  auto result = WaitForEvent(twiBaseAddress->EVENTS_RXSTARTED, DiagnosticError::StartTimeout);
  if (!result.IsOk()) {
    return AbortTransfer(result);
  }
  result = WaitForEvent(twiBaseAddress->EVENTS_LASTRX, DiagnosticError::TransferTimeout);
  if (!result.IsOk()) {
    return AbortTransfer(result);
  }
  return FinishTransfer(stop);
}

TwiMaster::DiagnosticResult TwiMaster::Write(uint8_t deviceAddress, const uint8_t* data, size_t size, bool stop) {
  twiBaseAddress->ADDRESS = deviceAddress;
  twiBaseAddress->TXD.PTR = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data));
  twiBaseAddress->TXD.MAXCNT = size;
  twiBaseAddress->TASKS_STARTTX = 1;
  twiBaseAddress->TASKS_RESUME = 1;
  auto result = WaitForEvent(twiBaseAddress->EVENTS_TXSTARTED, DiagnosticError::StartTimeout);
  if (!result.IsOk()) {
    return AbortTransfer(result);
  }
  result = WaitForEvent(twiBaseAddress->EVENTS_LASTTX, DiagnosticError::TransferTimeout);
  if (!result.IsOk()) {
    return AbortTransfer(result);
  }
  return FinishTransfer(stop);
}

void TwiMaster::Sleep() {
  twiBaseAddress->ENABLE = (TWIM_ENABLE_ENABLE_Disabled << TWIM_ENABLE_ENABLE_Pos);
}

void TwiMaster::Wakeup() {
  twiBaseAddress->ENABLE = (TWIM_ENABLE_ENABLE_Enabled << TWIM_ENABLE_ENABLE_Pos);
}

/* Sometimes, the TWIM device just freeze and never set the event EVENTS_LASTTX.
 * This method disable and re-enable the peripheral so that it works again.
 * This is just a workaround, and it would be better if we could find a way to prevent
 * this issue from happening.
 * */
void TwiMaster::FixHwFreezed() {
  NRF_LOG_INFO("I2C device frozen, reinitializing it!");

  const uint32_t twi_state = twiBaseAddress->ENABLE;

  Sleep();

  twiBaseAddress->ENABLE = twi_state;
}
