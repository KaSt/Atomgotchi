#include "ap_config.h"

WebServer server(80);
bool ap_mode_active = false;
unsigned long ap_start_time = 0;

// JSON parsing constants (compile-time)
const char JSON_DEVICE_NAME_KEY[] = "\"device_name\":\"";
const char JSON_BRIGHTNESS_KEY[] = "\"brightness\":";
const char JSON_PERSONALITY_KEY[] = "\"personality\":";
const size_t JSON_DEVICE_NAME_KEY_LEN = sizeof(JSON_DEVICE_NAME_KEY) - 1;
const size_t JSON_BRIGHTNESS_KEY_LEN = sizeof(JSON_BRIGHTNESS_KEY) - 1;
const size_t JSON_PERSONALITY_KEY_LEN = sizeof(JSON_PERSONALITY_KEY) - 1;

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Gotchi Configuration</title>
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
  <h1>Gotchi Configuration</h1>
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
    <legend>Personality:</legend>
      <div class="radio-group">
        <input type="radio" id="Friendly" name="personality" value="friendly" checked>
        <label for="friendly">Just looking for Friends</label>
        <input type="radio" id="Passive" name="personality" value="friendly">
        <label for="friendly">Passively sniffing around</label>
        <input type="radio" id="Aggressive" name="personality" value="friendly">
        <label for="friendly">Actively looking for any scent fo a WiFi</label>
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
          document.getElementById('personality').value = data.personality;
        });
    };
    
    document.getElementById('configForm').onsubmit = function(e) {
      e.preventDefault();
      
      const formData = new FormData(this);
      const data = {
        device_name: formData.get('device_name'),
        brightness: parseInt(formData.get('brightness')),
        personality: document.getElementById('personality').value
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
  json += "\"personality\":" + String(config->personality ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// Helper function to find end of JSON field value
int findJsonFieldEnd(const String& body, int start_pos) {
  int end_pos = body.indexOf(",", start_pos);
  if (end_pos == -1) {
    end_pos = body.indexOf("}", start_pos);
  }
  return end_pos;
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Simple JSON parsing (avoiding ArduinoJson for minimal dependencies)
    DeviceConfig* config = getConfig();
    
    // Parse device_name
    int name_start = body.indexOf(JSON_DEVICE_NAME_KEY);
    if (name_start >= 0) {
      name_start += JSON_DEVICE_NAME_KEY_LEN;
      int name_end = body.indexOf("\"", name_start);
      if (name_end > name_start) {
        String name = body.substring(name_start, name_end);
        strncpy(config->device_name, name.c_str(), sizeof(config->device_name) - 1);
        config->device_name[sizeof(config->device_name) - 1] = '\0';
      }
    }
    
    // Parse brightness with validation
    int bright_start = body.indexOf(JSON_BRIGHTNESS_KEY);
    if (bright_start >= 0) {
      bright_start += JSON_BRIGHTNESS_KEY_LEN;
      int bright_end = findJsonFieldEnd(body, bright_start);
      if (bright_end > bright_start) {
        String bright = body.substring(bright_start, bright_end);
        int brightness_value = bright.toInt();
        // Validate brightness is in range 0-255
        if (brightness_value >= 0 && brightness_value <= 255) {
          config->brightness = brightness_value;
        }
      }
    }
    
    // Parse personality
    int personality_start = body.indexOf(JSON_PERSONALITY_KEY);
    if (personality_start >= 0) {
      personality_start += JSON_PERSONALITY_KEY_LEN;
      int personality_end = findJsonFieldEnd(body, personality_start);
      if (personality_end > personality_start) {
        String personality_value = body.substring(personality_start, personality_end);
        personality_value.trim();
        if (personality_value == "Friendly") {
          config->personality = FRIENDLY;
        } else if (personality_value = "Passive") {
            config->personality = PASSIVE;
        } else if (personality_value = "Aggressive") {
            config->personality = AGGRESSIVE;
        } else {
          config->personality = FRIENDLY;
        }
      }
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
  Serial.println("AP Mode - Starting...");
  if (ap_mode_active) return;
  
  // Stop promiscuous mode and station mode
  Serial.println("Stop promiscuous mode and station mode");

  // CRITICAL: Stop promiscuous mode first
  esp_wifi_set_promiscuous(false);
  delay(100);  // Give it time to stop

  // Now start AP mode
  Serial.println("WiFi.mode(WIFI_AP)");
  WiFi.mode(WIFI_AP);
  delay(100);

  // Start Access Point
  Serial.println("Start Soft Access Point");
  bool ap_success = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (ap_success) {
    Serial.println("SoftAP started successfully.");
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    server.begin();
    ap_mode_active = true;
    ap_start_time = millis();
    Serial.println("AP Mode - Started.");
  } else {
      Serial.println("ERROR: Failed to start AP mode!");
      // Optionally: try to restart WiFi in station mode
      ap_mode_active = false;
  }
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
  
  // Auto-timeout after configured duration
  if (millis() - ap_start_time > AP_TIMEOUT_MS) {
    stopAPMode();
  }
}

bool isAPModeActive() {
  return ap_mode_active;
}

bool shouldExitAPMode() {
  // Check if AP mode was stopped (e.g., due to timeout)
  return !ap_mode_active;
}
