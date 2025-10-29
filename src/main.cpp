#include <Arduino.h>
#include "platform_config.h"
#include "processor.h"
#include "ota_server.h"

void setup() {
  // Initialize serial communication
  setupSerial(true, 115200, 1500);
  
  // Get platform-specific configuration and initialize processor
  ProcessorConfig cfg = getPlatformConfig();
  InitializeProcessor(cfg);

  // Setup OTA server (no-op on unsupported platforms)
  #if HAS_OTA_SUPPORT
    setupWiFi();
    setupOTA();
  #endif
}

void loop() {
  HandleSerialCLI();   // USB CLI (noop if nothing connected)
  ServiceProcessor();   // buttons, timed stop, phase machine
  
  // Service OTA functionality (no-op on unsupported platforms)
  serviceOTA();
}
