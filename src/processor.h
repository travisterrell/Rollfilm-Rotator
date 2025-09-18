#pragma once
#include <Arduino.h>

// If you already define LOGF/LOGFLN elsewhere, these won't override them.
#ifndef LOGF
  #define LOGF(...)   do { Serial.printf(__VA_ARGS__); } while(0)
#endif
#ifndef LOGFLN
  #define LOGFLN(...) do { Serial.printf(__VA_ARGS__); Serial.println(); } while(0)
#endif

struct ProcessorPins {
  int in1;       // DRV8871 IN1 (PWM-capable)
  int in2;       // DRV8871 IN2 (PWM-capable)
  int btnStart;  // active-low
  int btnStop;   // active-low
  int btnRes1;   // reserved (not used yet)
  int btnRes2;   // reserved (not used yet)
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
  // PWM configuration (auto-configured in main.cpp based on chip type)
  int pwmHz   = 20000;   // PWM frequency (ESP32: up to 20kHz, ESP8266: typically 1kHz for motors)
  int pwmBits = 11;      // duty 0..(2^bits-1) (ESP32: up to 12-bit, ESP8266: 10-bit)
  int chIn1   = 0;       // LEDC channel for IN1 (ESP32 only, ignored on ESP8266)
  int chIn2   = 1;       // LEDC channel for IN2 (ESP32 only, ignored on ESP8266)
  // Motion
  float cruisePct = 65.0f; // nominal duty %
  ProcessorTimings t;

  uint32_t defaultRunDurationMs = 0; // 0 = indefinite
};

void setDefaultRunDurationMs(uint32_t ms);

// Initialize pins, LEDC, buttons; coast the motor.
void initializeProcessor(const ProcessorConfig& cfg);

// Public primitives (drive↔coast mode)
void runForwardDuty(uint16_t duty);  // duty is 0..(2^pwmBits-1)
void runReverseDuty(uint16_t duty);
void coastStop();
void brakeStop();

// Start/stop high-level patterns
void startTimedCycle(uint32_t durationMs);  // 0 => indefinite
void startContinuousCycle();                // convenience for indefinite
void stopCycleCoast();                      // ramp down then coast
void stopCycleBrake();   // ramp optional -> brake, sets running=false & phase=IDLE

// Service functions (call from loop)
void serviceProcessor();   // buttons, timed stop, phase machine
void handleSerialCLI();   // optional USB CLI (noop if no data)
