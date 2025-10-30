#pragma once

#include <Arduino.h>
#include "platform_config.h"

// OTA and WiFi functionality - only compiled for supported platforms
// Provides no-op functions for unsupported platforms to keep main.cpp clean

#if HAS_OTA_SUPPORT
  // Platform-specific includes for ESP32-C6
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <AsyncTCP.h>
  #include <ElegantOTA.h>
  #include "web_dashboard.h"
  #include "processor.h"

  // WiFi Configuration
  #ifndef WIFI_SSID
    #define WIFI_SSID         "wifi_ssid"      // Normally declared via platformio.ini build_flags
  #endif
  #ifndef WIFI_PASSWORD  
    #define WIFI_PASSWORD     "wifi_password"  // Normally declared via platformio.ini build_flags
  #endif
#define OTA_PORT            80    // Web server port for OTA updates
  #define WIFI_TIMEOUT_MS   10000 // WiFi connection timeout

  // Global objects for OTA functionality
  extern AsyncWebServer server;
  extern AsyncWebSocket ws;
  extern unsigned long ota_progress_millis;
  extern unsigned long status_update_millis;

  // Function declarations
  void setupOTA();
  void serviceOTA();
  void setupWiFi();
  void broadcastStatus();
  void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, 
                 void *arg, uint8_t *data, size_t len);
  void onOTAStart();
  void onOTAProgress(size_t current, size_t final);
  void onOTAEnd(bool success);

#else
  // No-op functions for platforms without OTA support
  inline void setupOTA() { /* OTA not supported on this platform */ }
  inline void serviceOTA() { /* OTA not supported on this platform */ }
#endif