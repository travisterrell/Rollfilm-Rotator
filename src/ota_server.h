#pragma once

#include <Arduino.h>
#include "platform_config.h"

// OTA and WiFi functionality - only compiled when ENABLE_OTA is true
// Provides no-op functions for when ENABLE_OTA is false (though we have conditionals everywhere anyway)
#if ENABLE_OTA
  // Platform-specific includes
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncWebServer.h>
    #include <ESPAsyncTCP.h>
  #else
    #include <WiFi.h>
    #include <ESPAsyncWebServer.h>
    #include <AsyncTCP.h>
  #endif

  #include <ElegantOTA.h>
  #include "web_dashboard.h"
  #include "processor.h"

  // WiFi Configuration
  #ifndef WIFI_SSID
    #define WIFI_SSID "wifi_ssid"      // Normally declared via platformio.ini build_flags
  #endif
  #ifndef WIFI_PASSWORD  
    #define WIFI_PASSWORD "wifi_password"  // Normally declared via platformio.ini build_flags
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
  void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, 
                 void *arg, uint8_t *data, size_t len);
  void handleOTAStart();
  void onOTAProgress(size_t current, size_t final);
  void onOTAEnd(bool success);

#else
  // Redefine as no-op functions for platforms when ENABLE_OTA = false. (Though it should be superfluous since we already check it everywhere.)
  inline void setupOTA() {  }
  inline void serviceOTA() { }
#endif