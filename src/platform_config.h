#pragma once

#include <Arduino.h>
#include "processor.h"

// Platform detection and configuration
// Returns a ProcessorConfig struct with platform-specific pin assignments and PWM settings
inline ProcessorConfig getPlatformConfig() 
{
  ProcessorConfig cfg;
  
  //---------------------------------------------------------//  
  //       Common configuration for all platforms.           //
  //---------------------------------------------------------//
  // Control which rotation direction is forward/reverse by swapping these
  cfg.chIn1   = 0;  
  cfg.chIn2   = 1;

  // Speed configuration
  cfg.cruisePct = 72.3f; // nominal duty % (roughly equates to RPM on a 100 RPM gearmotor, but varies by load & voltage)

  // Timing configuration
  cfg.t.rampUpMs       = 15;
  cfg.t.rampDownMs     = 15;
  cfg.t.coastBetweenMs = 60;
  cfg.t.forwardRunMs   = 10000;
  cfg.t.reverseRunMs   = 10000;

  //---------------------------------------------------------//
  //  Platform-specific pin assignments & PWM configuration  //
  //---------------------------------------------------------//
  #if defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6 Super Mini friendly pins
    cfg.pins = {
        2, // motorPWM1
        3, // motorPWM2
        9  // toggleButton
    };
    cfg.pwmHz   = 1000; 
    cfg.pwmBits = 11;     // ESP32-C6 supports up to 14-bit PWM
  #elif defined(ESP32)
    // ESP32-WROOM-32 friendly pins (leave room for two more buttons and an I2C display)
    cfg.pins = {
        18, // motorPWM1
        19, // motorPWM2
        25  // toggleButton
    };
    cfg.pwmHz   = 20000;
    cfg.pwmBits = 11; // ESP32 couldn't handle 12 bits @ 20kHz
  #elif defined(ESP8266)
    // ESP8266 D1 Mini friendly pins
    cfg.pins = {
        D1,  // motorPWM1
        D2,  // motorPWM2
        D5   // toggleButton
    };
    cfg.pwmHz   = 1000;  // ESP8266 PWM frequency
    cfg.pwmBits = 10;    // ESP8266 supports 10-bit PWM (0-1023)
  #endif

  return cfg;
}

// Handle OTA default
#ifndef ENABLE_OTA
  #define ENABLE_OTA 0
#endif