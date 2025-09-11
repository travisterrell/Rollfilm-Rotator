#include <Arduino.h>
#include "processor.h"

// --- Serial config (make headless easy) ---
#define ENABLE_SERIAL       1
#define WAIT_FOR_SERIAL_MS  2000

#if ENABLE_SERIAL
  #define LOG_BAUD 115200
#else
  #undef LOGF
  #undef LOGFLN
  #define LOGF(...)   do{}while(0)
  #define LOGFLN(...) do{}while(0)
#endif

constexpr uint32_t minutes(uint32_t m){ return m * 60UL * 1000UL; }
constexpr uint32_t seconds(uint32_t s){ return s * 1000UL; }

void setup() {
#if ENABLE_SERIAL
  Serial.begin(LOG_BAUD);
  if (WAIT_FOR_SERIAL_MS > 0) {
    unsigned long t0 = millis();
    while (!Serial && (millis() - t0) < WAIT_FOR_SERIAL_MS) { /* optional wait */ }
  }
#endif

  // Pick ESP32-WROOM-32 friendly pins (leave room for two more buttons and an I2C display)
  ProcessorConfig cfg;
  cfg.pins = {
    /* motorPWM1   */ 18,
    /* motorPWM2   */ 19,
    /* startButton */ 25,
    /* stopButton  */ 26,
    /* button3  */ 27,
    /* button4  */ 14
  };
  cfg.pwmHz   = 20000;
  cfg.pwmBits = 12;
  cfg.chIn1   = 0;
  cfg.chIn2   = 1;

  cfg.cruisePct = 65.0f;
  cfg.t.rampUpMs       = 300;
  cfg.t.rampDownMs     = 200;
  cfg.t.coastBetweenMs = 500;
  cfg.t.forwardRunMs   = 10000;
  cfg.t.reverseRunMs   = 10000;

  // cfg.defaultRunDurationMs = seconds(120); // Commented out defaults to 0, which is continuous
  

  initializeProcessor(cfg);

  // Start automatically for quick bench tests (optional)
  // startContinuousCycle();
  // startTimedCycle(30UL * 60UL * 1000UL);  // 30 minutes
}

void loop() {
  handleSerialCLI();   // USB CLI (noop if nothing connected)
  serviceProcessor();   // buttons, timed stop, phase machine
}
