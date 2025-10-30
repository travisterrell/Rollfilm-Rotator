#include "ota_server.h"
#include <cstdarg>
#include <cstdio>

#if ENABLE_OTA

// Global objects for OTA functionality
AsyncWebServer server(OTA_PORT);
AsyncWebSocket ws("/ws");
unsigned long ota_progress_millis = 0;
unsigned long status_update_millis = 0;
constexpr size_t LOG_HISTORY_SIZE = 50;

struct LogEntry 
{
  String message;
  uint32_t timestamp;
};

static LogEntry logHistory[LOG_HISTORY_SIZE];
static size_t logHistoryCount = 0;
static size_t logHistoryHead = 0;

static String escapeJson(const String &input)
{
  String escaped;
  escaped.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i)
  {
    const char c = input[i];
    switch (c)
    {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += c;
      break;
    }
  }
  return escaped;
}

static String buildLogPayload(const LogEntry &entry)
{
  String payload = "{\"type\":\"log\",\"timestamp\":";
  payload += String(entry.timestamp);
  payload += ",\"message\":\"";
  payload += escapeJson(entry.message);
  payload += "\"}";
  return payload;
}

static void storeLogEntry(const String &message, uint32_t timestamp)
{
  logHistory[logHistoryHead].message = message;
  logHistory[logHistoryHead].timestamp = timestamp;
  logHistoryHead = (logHistoryHead + 1) % LOG_HISTORY_SIZE;
  if (logHistoryCount < LOG_HISTORY_SIZE)
  {
    ++logHistoryCount;
  }
}

static void sendLogHistoryToClient(AsyncWebSocketClient *client)
{
  if (!client || logHistoryCount == 0)
    return;

  size_t oldestIndex = (logHistoryHead + LOG_HISTORY_SIZE - logHistoryCount) % LOG_HISTORY_SIZE;
  for (size_t i = 0; i < logHistoryCount; ++i)
  {
    const LogEntry &entry = logHistory[(oldestIndex + i) % LOG_HISTORY_SIZE];
    client->text(buildLogPayload(entry));
  }
}

void OtaLogLinef(const char *fmt, ...)
{
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  const uint32_t now = millis();
  String message(buffer);
  storeLogEntry(message, now);

  if (ws.count() > 0)
  {
    LogEntry entry{message, now};
    ws.textAll(buildLogPayload(entry));
  }
}

// OTA Progress callbacks
void onOTAStart()
{
  Serial.println("OTA update started!");
  StopCycleBrake();
}

void onOTAProgress(size_t current, size_t final)
{
  // Log progress every second
  if (millis() - ota_progress_millis > 1000)
  {
    ota_progress_millis = millis();
    float progress = ((float)current / (float) final) * 100.0f;

    Serial.printf("OTA Progress: %.1f%% (%u/%u bytes)\n", progress, current, final);
  }
}

void onOTAEnd(bool success)
{
  if (success)
  {
    Serial.println("OTA update completed successfully! Rebooting...");
  }
  else
  {
    Serial.println("OTA update failed!");
  }
}

// WebSocket event handler
void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client connected: %u\n", client->id());
      sendLogHistoryToClient(client);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client disconnected: %u\n", client->id());
      break;
    case WS_EVT_DATA:
    {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
      {
        String command = "";
        for (size_t i = 0; i < len; i++)
        {
          command += (char)data[i];
        }
        
        Serial.printf("WebSocket command received: %s\n", command.c_str());

        // Handle remote commands
        if (command == "start" || command == "auto_start")
        {
          ProcessorCommandAutoStart();
        }
        else if (command == "stop" || command == "stop_brake")
        {
          ProcessorCommandBrakeStop();
        }
        else if (command == "coast" || command == "stop_coast")
        {
          ProcessorCommandCoastStop();
        }
        else if (command == "manual_fwd")
        {
          ProcessorCommandManualForward();
        }
        else if (command == "manual_rev")
        {
          ProcessorCommandManualReverse();
        }
        else if (command == "print_status" || command == "status")
        {
          ProcessorCommandPrintState();
        }
        else if (command == "test_in1")
        {
          ProcessorCommandTestIn1();
        }
        else if (command == "test_in2")
        {
          ProcessorCommandTestIn2();
        }
        else if (command == "motors_off")
        {
          ProcessorCommandAllOff();
        }
        else if (command.startsWith("set_cruise="))
        {
          const String value = command.substring(String("set_cruise=").length());
          float pct = value.toFloat();
          ProcessorCommandSetCruise(pct);
        }
        else
        {
          Serial.printf("Unknown WebSocket command: %s\n", command.c_str());
        }
      }
      break;
    }
    default:
      break;
    }
}

// Broadcast status updates to all connected WebSocket clients
void broadcastStatus()
{
  if (ws.count() > 0)
  {
    String status = "{\"type\":\"status\",";
    status += "\"uptime\":" + String(millis()) + ",";
    status += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    status += "\"wifi_rssi\":" + String(WiFi.RSSI());
    status += "}";
    ws.textAll(status);
  }
}

void setupWiFi()
{
  Serial.printf("Connecting to WiFi network: %s", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.printf("WiFi connected! IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("OTA interface available at: http://%s/update\n", WiFi.localIP().toString().c_str());
    Serial.printf("Live dashboard available at: http://%s/\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.println("");
    Serial.println("WiFi connection failed - OTA updates unavailable");
  }
}

void setupOTA()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected - skipping OTA setup");
    return;
  }

  // Live dashboard with real-time updates
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", getDashboardHTML()); });

  // JSON API endpoints
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = "{";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    request->send(200, "application/json", json); });

  // WebSocket setup
  ws.onEvent(handleWebSocketEvent);
  server.addHandler(&ws);

  // Initialize ElegantOTA
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("Async OTA server started with live dashboard");
}

void serviceOTA()
{
  // Send status updates every 2 seconds
  if (millis() - status_update_millis > 2000)
  {
    status_update_millis = millis();
    broadcastStatus();
  }

  // Clean up WebSocket connections
  ws.cleanupClients();

  ElegantOTA.loop(); // Handle OTA updates
}

#else

void OtaLogLinef(const char *fmt, ...)
{
  (void)fmt; // OTA disabled, ignore mirrored logs
}

#endif // ENABLE_OTA