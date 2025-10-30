#pragma once

// HTML dashboard content for the OTA web interface
// Separated from ota_server.cpp to keep the logic clean

inline const char *getDashboardHTML()
{
    return R"html(
            <!DOCTYPE html>
            <html>
            <head>
                <title>Rollfilm Rotator Dashboard</title>
                <meta name="viewport" content="width=device-width, initial-scale=1">
                <style>
                    body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
                    .container { max-width: 720px; margin: 0 auto; background: white; padding: 24px; border-radius: 12px; box-shadow: 0 6px 18px rgba(0,0,0,0.08); }
                    h1, h3 { margin-top: 24px; }
                    .status { display: flex; justify-content: space-between; margin: 6px 0; font-size: 14px; }
                    .button-row { display: flex; flex-wrap: wrap; gap: 10px; margin: 12px 0; }
                    .button { background: #4CAF50; color: white; padding: 10px 16px; border: none; border-radius: 6px; cursor: pointer; transition: background 0.2s ease, transform 0.1s ease; }
                    .button:hover { background: #45a049; transform: translateY(-1px); }
                    .button:disabled { background: #a5a5a5; cursor: not-allowed; }
                    .stop { background: #f44336; }
                    .stop:hover { background: #da190b; }
                    .secondary { background: #1976d2; }
                    .secondary:hover { background: #135ba4; }
                    .outline { background: #ffffff; color: #333; border: 1px solid #ccc; }
                    .outline:hover { background: #f2f2f2; }
                    .form-row { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; margin: 12px 0; }
                    .form-row label { font-size: 14px; }
                    input[type="number"] { width: 90px; padding: 6px 8px; border-radius: 4px; border: 1px solid #bbb; }
                    #log { background: #1e1e1e; color: #8ef58e; padding: 10px; min-height: 200px; max-height: 260px; overflow-y: auto; font-family: "Fira Code", monospace; font-size: 12px; border-radius: 6px; margin-top: 8px; }
                    #log div { margin-bottom: 4px; }
                    a { color: #1976d2; text-decoration: none; }
                    a:hover { text-decoration: underline; }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>&#x1F3AC; Rollfilm Rotator</h1>
                    <div class="status"><span>Uptime:</span><span id="uptime">-</span></div>
                    <div class="status"><span>Free Heap:</span><span id="heap">-</span></div>
                    <div class="status"><span>WiFi RSSI:</span><span id="rssi">-</span></div>
                    
                    <h3>Motor Control</h3>
                    <div class="button-row">
                        <button class="button secondary" onclick="sendCommand('auto_start')">Start Auto Cycle</button>
                        <button class="button stop" onclick="sendCommand('stop_brake')">Stop (Brake)</button>
                        <button class="button outline" onclick="sendCommand('stop_coast')">Stop (Coast)</button>
                    </div>

                    <h3>Manual Jog</h3>
                    <div class="button-row">
                        <button class="button" onclick="sendCommand('manual_fwd')">Jog Forward</button>
                        <button class="button" onclick="sendCommand('manual_rev')">Jog Reverse</button>
                        <button class="button outline" onclick="sendCommand('motors_off')">All Off</button>
                    </div>

                    <h3>Tuning & Status</h3>
                    <div class="form-row">
                        <label for="cruiseInput">Cruise %</label>
                        <input type="number" id="cruiseInput" value="72.0" min="0" max="100" step="0.5">
                        <button class="button" onclick="applyCruise()">Apply</button>
                        <button class="button outline" onclick="sendCommand('print_status')">Print Status</button>
                    </div>

                    <h3>Diagnostics</h3>
                    <div class="button-row">
                        <button class="button outline" onclick="sendCommand('test_in1')">Test IN1 (Forward)</button>
                        <button class="button outline" onclick="sendCommand('test_in2')">Test IN2 (Reverse)</button>
                    </div>
                    
                    <h3>Live Log</h3>
                    <div id="log"></div>
                    
                    <p><a href="/update">&#x1F503; Firmware Update</a></p>
                </div>

                <script>
                    const ws = new WebSocket('ws://' + window.location.host + '/ws');
                    const log = document.getElementById('log');
                    const MAX_LOG_LINES = 200;
                    const cruiseInput = document.getElementById('cruiseInput');

                    function appendLogLine(text) {
                        const entry = document.createElement('div');
                        entry.textContent = text;
                        log.appendChild(entry);
                        while (log.children.length > MAX_LOG_LINES) {
                            log.removeChild(log.firstChild);
                        }
                        log.scrollTop = log.scrollHeight;
                    }

                    ws.onmessage = function(event) {
                        const data = JSON.parse(event.data);
                        if (data.type === 'status') {
                            document.getElementById('uptime').textContent = (data.uptime / 1000).toFixed(1) + 's';
                            document.getElementById('heap').textContent = data.heap + ' bytes';
                            document.getElementById('rssi').textContent = data.wifi_rssi + ' dBm';
                        } else if (data.type === 'log') {
                            const deviceTime = (data.timestamp / 1000).toFixed(1) + 's';
                            appendLogLine('[' + deviceTime + '] ' + data.message);
                        }
                    };

                    function sendCommand(cmd, value) {
                        let message = cmd;
                        if (typeof value !== 'undefined') {
                            message += '=' + value;
                        }
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.send(message);
                        } else {
                            appendLogLine(new Date().toLocaleTimeString() + ' - WebSocket not connected');
                        }
                        appendLogLine(new Date().toLocaleTimeString() + ' - Sent: ' + message);
                    }

                    function applyCruise() {
                        const value = parseFloat(cruiseInput.value);
                        if (isNaN(value)) {
                            appendLogLine(new Date().toLocaleTimeString() + ' - Invalid cruise percentage');
                            return;
                        }
                        const clamped = Math.min(Math.max(value, 0), 100).toFixed(1);
                        cruiseInput.value = clamped;
                        sendCommand('set_cruise', clamped);
                    }

                    ws.onopen = function() {
                        appendLogLine(new Date().toLocaleTimeString() + ' - Connected to device');
                    };

                    ws.onclose = function() {
                        appendLogLine(new Date().toLocaleTimeString() + ' - Connection closed');
                    };
                </script>
            </body>
            </html>
            )html";
}