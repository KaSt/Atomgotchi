#include "ap_config.h"
#include "M5Unified.h"

WebServer server(80);
bool ap_mode_active = false;
unsigned long ap_start_time = 0;

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Palnagotchi Configuration</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      max-width: 500px; 
      margin: 50px auto; 
      padding: 20px;
      background: #1a1a1a;
      color: #00ff00;
    }
    h1 { 
      text-align: center; 
      color: #00ff00;
      border-bottom: 2px solid #00ff00;
      padding-bottom: 10px;
    }
    .form-group { 
      margin: 20px 0; 
    }
    label { 
      display: block; 
      margin-bottom: 5px; 
      font-weight: bold;
    }
    input[type="text"], input[type="number"] { 
      width: 100%; 
      padding: 10px; 
      font-size: 16px; 
      border: 1px solid #00ff00;
      background: #0a0a0a;
      color: #00ff00;
      box-sizing: border-box;
    }
    input[type="checkbox"] {
      width: 20px;
      height: 20px;
      margin-right: 10px;
    }
    button { 
      width: 100%; 
      padding: 12px; 
      font-size: 16px; 
      background: #00ff00; 
      color: #000; 
      border: none; 
      cursor: pointer; 
      font-weight: bold;
      margin-top: 10px;
    }
    button:hover { 
      background: #00dd00; 
    }
    .message {
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #00ff00;
      text-align: center;
    }
    .checkbox-group {
      display: flex;
      align-items: center;
    }
  </style>
</head>
<body>
  <h1>⚙️ Palnagotchi Config</h1>
  <form id="configForm">
    <div class="form-group">
      <label for="device_name">Device Name:</label>
      <input type="text" id="device_name" name="device_name" maxlength="31" required>
    </div>
    <div class="form-group">
      <label for="brightness">Display Brightness (0-255):</label>
      <input type="number" id="brightness" name="brightness" min="0" max="255" required>
    </div>
    <div class="form-group">
      <div class="checkbox-group">
        <input type="checkbox" id="sound_enabled" name="sound_enabled">
        <label for="sound_enabled">Enable Sound</label>
      </div>
    </div>
    <button type="submit">Save Configuration</button>
    <button type="button" onclick="resetConfig()">Reset to Defaults</button>
  </form>
  <div id="message" class="message" style="display:none;"></div>
  
  <script>
    // Load current configuration on page load
    window.onload = function() {
      fetch('/api/config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('device_name').value = data.device_name;
          document.getElementById('brightness').value = data.brightness;
          document.getElementById('sound_enabled').checked = data.sound_enabled;
        });
    };
    
    document.getElementById('configForm').onsubmit = function(e) {
      e.preventDefault();
      
      const formData = new FormData(this);
      const data = {
        device_name: formData.get('device_name'),
        brightness: parseInt(formData.get('brightness')),
        sound_enabled: document.getElementById('sound_enabled').checked
      };
      
      fetch('/api/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      })
      .then(response => response.json())
      .then(result => {
        const msg = document.getElementById('message');
        msg.textContent = result.message;
        msg.style.display = 'block';
        setTimeout(() => { msg.style.display = 'none'; }, 3000);
      });
    };
    
    function resetConfig() {
      if (confirm('Reset all settings to defaults?')) {
        fetch('/api/reset', { method: 'POST' })
          .then(response => response.json())
          .then(result => {
            const msg = document.getElementById('message');
            msg.textContent = result.message;
            msg.style.display = 'block';
            setTimeout(() => { 
              location.reload(); 
            }, 2000);
          });
      }
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", html_page);
}

void handleGetConfig() {
  DeviceConfig* config = getConfig();
  String json = "{";
  json += "\"device_name\":\"" + String(config->device_name) + "\",";
  json += "\"brightness\":" + String(config->brightness) + ",";
  json += "\"sound_enabled\":" + String(config->sound_enabled ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Simple JSON parsing (avoiding ArduinoJson for minimal dependencies)
    DeviceConfig* config = getConfig();
    
    // Parse device_name
    int name_start = body.indexOf("\"device_name\":\"") + 15;
    int name_end = body.indexOf("\"", name_start);
    if (name_start > 14 && name_end > name_start) {
      String name = body.substring(name_start, name_end);
      strncpy(config->device_name, name.c_str(), sizeof(config->device_name) - 1);
      config->device_name[sizeof(config->device_name) - 1] = '\0';
    }
    
    // Parse brightness
    int bright_start = body.indexOf("\"brightness\":") + 13;
    int bright_end = body.indexOf(",", bright_start);
    if (bright_start > 12) {
      String bright = body.substring(bright_start, bright_end);
      config->brightness = bright.toInt();
    }
    
    // Parse sound_enabled
    int sound_pos = body.indexOf("\"sound_enabled\":");
    if (sound_pos > 0) {
      config->sound_enabled = body.indexOf("true", sound_pos) > 0;
    }
    
    saveConfig();
    
    server.send(200, "application/json", "{\"message\":\"Configuration saved! Changes will apply on restart.\"}");
  } else {
    server.send(400, "application/json", "{\"message\":\"Invalid request\"}");
  }
}

void handleResetConfig() {
  resetConfig();
  saveConfig();
  server.send(200, "application/json", "{\"message\":\"Configuration reset to defaults!\"}");
}

void initAPConfig() {
  server.on("/", handleRoot);
  server.on("/api/config", handleGetConfig);
  server.on("/api/save", HTTP_POST, handleSaveConfig);
  server.on("/api/reset", HTTP_POST, handleResetConfig);
}

void startAPMode() {
  if (ap_mode_active) return;
  
  // Stop promiscuous mode and station mode
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_AP);
  
  // Start Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();
  ap_mode_active = true;
  ap_start_time = millis();
}

void stopAPMode() {
  if (!ap_mode_active) return;
  
  server.stop();
  WiFi.softAPdisconnect(true);
  ap_mode_active = false;
  
  // Reinitialize WiFi for pwngrid mode
  WiFi.mode(WIFI_OFF);
  delay(100);
}

void handleAPConfig() {
  if (!ap_mode_active) return;
  
  server.handleClient();
  
  // Auto-timeout after AP_TIMEOUT_MS
  if (millis() - ap_start_time > AP_TIMEOUT_MS) {
    stopAPMode();
  }
}

bool isAPModeActive() {
  return ap_mode_active;
}
