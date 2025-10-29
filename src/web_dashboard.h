#pragma once

// HTML dashboard content for the OTA web interface
// Separated from ota_server.cpp to keep the logic clean

inline const char* getDashboardHTML() {
  return R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Rollfilm Rotator Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        .status { display: flex; justify-content: space-between; margin: 10px 0; }
        .button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }
        .button:hover { background: #45a049; }
        .stop { background: #f44336; } .stop:hover { background: #da190b; }
        #log { background: #333; color: #0f0; padding: 10px; height: 200px; overflow-y: scroll; font-family: monospace; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎬 Rollfilm Rotator</h1>
        <div class="status"><span>Uptime:</span><span id="uptime">-</span></div>
        <div class="status"><span>Free Heap:</span><span id="heap">-</span></div>
        <div class="status"><span>WiFi RSSI:</span><span id="rssi">-</span></div>
        
        <h3>Motor Control</h3>
        <button class="button" onclick="sendCommand('start')">Start Cycle</button>
        <button class="button stop" onclick="sendCommand('stop')">Stop (Brake)</button>
        <button class="button" onclick="sendCommand('coast')">Stop (Coast)</button>
        
        <h3>Live Log</h3>
        <div id="log"></div>
        
        <p><a href="/update">🔄 Firmware Update</a></p>
    </div>

    <script>
        const ws = new WebSocket('ws://' + window.location.host + '/ws');
        const log = document.getElementById('log');
        
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            document.getElementById('uptime').textContent = (data.uptime / 1000).toFixed(1) + 's';
            document.getElementById('heap').textContent = data.heap + ' bytes';
            document.getElementById('rssi').textContent = data.wifi_rssi + ' dBm';
        };
        
        function sendCommand(cmd) {
            ws.send(cmd);
            log.innerHTML += new Date().toLocaleTimeString() + ' - Sent: ' + cmd + '<br>';
            log.scrollTop = log.scrollHeight;
        }
        
        ws.onopen = function() {
            log.innerHTML += new Date().toLocaleTimeString() + ' - Connected to device<br>';
        };
    </script>
</body>
</html>
)html";
}