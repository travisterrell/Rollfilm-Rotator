#include <Arduino.h>
#include "processor.h"

// --- Serial config (make headless easy) ---
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

  // Auto-detect chip and configure pins accordingly
  ProcessorConfig cfg;
#if defined(CONFIG_IDF_TARGET_ESP32C6)
  // ESP32-C6 Super Mini friendly pins
  cfg.pins = {
    /* motorPWM1   */ 2,   // GPIO2 - PWM capable
    /* motorPWM2   */ 3,   // GPIO3 - PWM capable
    /* toggleButton */ 9,  // GPIO9 - safe input pin
    /* stopButton  */ 10,  // GPIO10 - safe input pin
  };
  cfg.pwmHz   = 20000;  // ESP32-C6 supports high frequency PWM
  cfg.pwmBits = 11;     // ESP32-C6 supports up to 14-bit PWM
#elif defined(ESP32)
  // ESP32-WROOM-32 friendly pins (leave room for two more buttons and an I2C display)
  cfg.pins = {
    /* motorPWM1   */ 18,
    /* motorPWM2   */ 19,
    /* toggleButton */ 25,
    /* stopButton  */ 26,
  };
  cfg.pwmHz   = 20000;
  cfg.pwmBits = 11; // ESP32 couldn't handle 12 bits @ 20kHz
#elif defined(ESP8266)
  // ESP8266 D1 Mini friendly pins
  cfg.pins = {
    /* motorPWM1   */ D1,  // GPIO5
    /* motorPWM2   */ D2,  // GPIO4
    /* toggleButton */ D5, // GPIO14
    /* stopButton  */ D6,  // GPIO12
  };
  cfg.pwmHz   = 1000;  // ESP8266 PWM frequency (1kHz is good for motors)
  cfg.pwmBits = 10;    // ESP8266 supports 10-bit PWM (0-1023)
#endif
  cfg.chIn1   = 0;
  cfg.chIn2   = 1;

  cfg.cruisePct = 72.0f;
  cfg.t.rampUpMs       = 10;
  cfg.t.rampDownMs     = 10;
  cfg.t.coastBetweenMs = 50;
  cfg.t.forwardRunMs   = 15000;
  cfg.t.reverseRunMs   = 15000;

  // cfg.defaultRunDurationMs = SecondsToMs(120); // Commented out defaults to 0, which is continuous

  InitializeProcessor(cfg);

}

void loop() {
  HandleSerialCLI();   // USB CLI (noop if nothing connected)
  ServiceProcessor();   // buttons, timed stop, phase machine
}
