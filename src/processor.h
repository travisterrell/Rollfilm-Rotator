#pragma once
#include <Arduino.h>

// Optional web dashboard log mirroring (implemented in ota_server.cpp when ENABLE_OTA=1)
void OtaLogLinef(const char *fmt, ...);

// If LOGF/LOGFLN defined elsewhere, these won't override them.
#ifndef LOGF
  #define LOGF(...)   do { Serial.printf(__VA_ARGS__); } while(0)
#endif
#ifndef LOGFLN
  #define LOGFLN(...) do { Serial.printf(__VA_ARGS__); Serial.println(); OtaLogLinef(__VA_ARGS__); } while(0)
#endif

struct ProcessorPins {
  int in1;       // DRV8871 IN1
  int in2;       // DRV8871 IN2
  int btnStart;  // active-low
};

struct ProcessorTimings {
  uint16_t rampUpMs      = 300;
  uint16_t rampDownMs    = 200;
  uint16_t coastBetweenMs= 500;
  uint32_t forwardRunMs  = 10000; // 10 s
  uint32_t reverseRunMs  = 10000; // 10 s
};

struct ProcessorConfig {
  ProcessorPins pins;
  // PWM configuration (configured in main.cpp based on chip type in platformio.ini)
  int pwmHz   = 20000;   // PWM frequency (ESP32/ESP32-C6: up to 20kHz, ESP8266: typically 1kHz for motors)
  int pwmBits = 11;      // duty 0..(2^bits-1) (ESP32/ESP32-C6: up to 14-bit, ESP8266: 10-bit)
  int chIn1   = 0;       // LEDC channel for IN1 (ESP32/ESP32-C6 only, ignored on ESP8266)
  int chIn2   = 1;       // LEDC channel for IN2 (ESP32/ESP32-C6 only, ignored on ESP8266)
  // Motion
  float cruisePct = 65.0f; // nominal duty %
  ProcessorTimings t;
};

// Initialize pins, LEDC, buttons; coast the motor.
void InitializeProcessor(const ProcessorConfig& cfg);

// Public primitives (drive/coast/brake modes)
void RunForwardDuty(uint16_t duty);  // duty is 0..(2^pwmBits-1)
void RunReverseDuty(uint16_t duty);
void CoastStop();
void BrakeStop();

// Start/stop high-level patterns
void StartContinuousCycle();  // begin alternating forward/reverse pattern
void StopCycleCoast();        // ramp down then coast
void StopCycleBrake();        // brake, sets running=false & phase=IDLE

// Service functions (call from loop)
void ServiceProcessor();      // buttons, timed stop, phase machine
void HandleSerialCLI();       // optional USB CLI (noop if no data)

// Serial setup and configuration
void setupSerial(bool waitForSerial = true, uint32_t baudRate = 115200, uint32_t waitTimeMs = 1500);

// Shared command helpers (used by serial CLI and OTA dashboard)
void ProcessorCommandManualForward();
void ProcessorCommandManualReverse();
void ProcessorCommandCoastStop();
void ProcessorCommandBrakeStop();
void ProcessorCommandAutoStart();
void ProcessorCommandSetCruise(float pct);
void ProcessorCommandPrintState();
void ProcessorCommandTestIn1();
void ProcessorCommandTestIn2();
void ProcessorCommandAllOff();
