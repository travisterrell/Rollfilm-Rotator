#include <Arduino.h>
#include "processor.h"

// --- Serial debugging config ---
#define ENABLE_SERIAL       1
#define WAIT_FOR_SERIAL_MS  1500

#if ENABLE_SERIAL
  #define LOG_BAUD 115200
#else
  #undef LOGF
  #undef LOGFLN
  #define LOGF(...)   do{}while(0)
  #define LOGFLN(...) do{}while(0)
#endif

constexpr uint32_t MinutesToMs(uint32_t m){ return m * 60UL * 1000UL; }
constexpr uint32_t SecondsToMs(uint32_t s){ return s * 1000UL; }

void setup() {
#if ENABLE_SERIAL
  Serial.begin(LOG_BAUD);
  if (WAIT_FOR_SERIAL_MS > 0) {
    unsigned long t0 = millis();
    while (!Serial && (millis() - t0) < WAIT_FOR_SERIAL_MS) { /* optional wait */ }
  }
#endif

  // Configure pins based on specified chip platform
  ProcessorConfig cfg;
#if defined(CONFIG_IDF_TARGET_ESP32C6)
  // ESP32-C6 Super Mini friendly pins
  cfg.pins = {
    2, // motorPWM1
    3, // motorPWM2
    9, // toggleButton
    10, // stopButton
  };
  cfg.pwmHz   = 1000; 
  cfg.pwmBits = 11;     // ESP32-C6 supports up to 14-bit PWM
#elif defined(ESP32)
  // ESP32-WROOM-32 friendly pins (leave room for two more buttons and an I2C display)
  cfg.pins = {
    18, // motorPWM1
    19, // motorPWM2
    25, // toggleButton
    26, // stopButton
  };
  cfg.pwmHz   = 20000;
  cfg.pwmBits = 11; // ESP32 couldn't handle 12 bits @ 20kHz
#elif defined(ESP8266)
  // ESP8266 D1 Mini friendly pins
  cfg.pins = {
    D1,  // motorPWM1
    D2,  // motorPWM2
    D5,  // toggleButton
    D6,  // stopButton
  };
  cfg.pwmHz   = 1000;  // ESP8266 PWM frequency
  cfg.pwmBits = 10;    // ESP8266 supports 10-bit PWM (0-1023)
#endif

  cfg.chIn1   = 0;  
  cfg.chIn2   = 1;

  cfg.cruisePct = 72.3f; // nominal duty % (roughly equates to RPM on a 100 RPM gearmotor)

  cfg.t.rampUpMs       = 15;
  cfg.t.rampDownMs     = 15;
  cfg.t.coastBetweenMs = 60;
  cfg.t.forwardRunMs   = 10000;
  cfg.t.reverseRunMs   = 10000;

  InitializeProcessor(cfg);

}

void loop() {
  HandleSerialCLI();   // USB CLI (noop if nothing connected)
  ServiceProcessor();   // buttons, timed stop, phase machine
}
