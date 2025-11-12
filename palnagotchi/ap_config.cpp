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
  <h1>Welcome to Gotchu</h1>
  <div style="margin-top:16px">
    <a href="/friends">Friends</a> &nbsp;|&nbsp;
    <a href="/packets">Packets</a>
  </div>
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

static String ndjsonToJsonArray(const char* path, size_t limit = 0) {
  File f = LittleFS.open(path, FILE_READ);
  String out = "[";
  if (!f) { out += "]"; return out; }

  StaticJsonDocument<1024> doc;
  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.isEmpty()) continue;
    doc.clear();
    DeserializationError err = deserializeJson(doc, line);
    if (err) continue;

    if (!first) out += ","; else first = false;
    String tmp; serializeJson(doc, tmp);
    out += tmp;

    if (limit && !--limit) break;
  }
  f.close();
  out += "]";
  Serial.println("njsontoarray: " + out);
  return out;
}

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

static bool sendBytesAsFile(const String& filename, const uint8_t* data, size_t len) {
  WiFiClient client = server.client();
  String hdr = "HTTP/1.1 200 OK\r\n";
  hdr += "Content-Type: application/octet-stream\r\n";
  hdr += "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";
  hdr += "Content-Length: " + String(len) + "\r\n\r\n";
  client.print(hdr);
  size_t sent = client.write(data, len);
  return sent == len;
}

static void handleDownloadPacket() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  const String wantId = server.arg("id");

  File f = LittleFS.open(PK_TBL, FILE_READ);
  if (!f) { server.send(404, "text/plain", "packets table not found"); return; }

  StaticJsonDocument<4096> doc;  // aumenta se i record hanno b64 grandi
  bool served = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.isEmpty()) continue;
    doc.clear();
    if (deserializeJson(doc, line)) continue;

    // id come stringa (qualsiasi tipo nel JSON)
    String idStr;
    if (doc["id"].is<const char*>()) idStr = String((const char*)doc["id"]);
    else if (doc["id"].is<long>())   idStr = String((long)doc["id"]);
    else if (doc["id"].is<unsigned long>()) idStr = String((unsigned long)doc["id"]);
    else if (doc["id"].is<int>())    idStr = String((int)doc["id"]);
    else if (doc["id"].is<uint32_t>()) idStr = String((uint32_t)doc["id"]);

    if (idStr != wantId) continue;

    // Caso 1: contenuto inline base64
    if (doc.containsKey("hc22000_b64")) {
      String b64 = doc["hc22000_b64"].as<String>();
      size_t needed = 0;
      // calcola lunghezza decodificata
      int rc = mbedtls_base64_decode(nullptr, 0, &needed,
                                     (const unsigned char*)b64.c_str(), b64.length());
      if (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && rc != 0) {
        server.send(500, "text/plain", "base64 length error");
        break;
      }
      std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[needed]);
      if (!buf) { server.send(500, "text/plain", "oom"); break; }

      size_t outLen = 0;
      rc = mbedtls_base64_decode(buf.get(), needed, &outLen,
                                 (const unsigned char*)b64.c_str(), b64.length());
      if (rc != 0) { server.send(500, "text/plain", "base64 decode error"); break; }

      const String fname = "packet_" + idStr + ".hc22000";
      served = sendBytesAsFile(fname, buf.get(), outLen);
      break;
    }

    // Caso 2: path file su LittleFS
    if (doc.containsKey("file")) {
      String p = doc["file"].as<String>();
      if (!LittleFS.exists(p)) { server.send(404, "text/plain", "file not found"); break; }

      File pf = LittleFS.open(p, FILE_READ);
      if (!pf) { server.send(500, "text/plain", "cannot open file"); break; }

      WiFiClient client = server.client();
      const String fname = (p.endsWith(".hc22000") ? p.substring(p.lastIndexOf('/')+1)
                                                   : "packet_"+idStr+".hc22000");
      String hdr = "HTTP/1.1 200 OK\r\n";
      hdr += "Content-Type: application/octet-stream\r\n";
      hdr += "Content-Disposition: attachment; filename=\"" + fname + "\"\r\n";
      hdr += "Content-Length: " + String(pf.size()) + "\r\n\r\n";
      client.print(hdr);

      uint8_t buf[1024];
      while (pf.available()) {
        size_t r = pf.read(buf, sizeof(buf));
        if (!r) break;
        client.write(buf, r);
      }
      pf.close();
      served = true;
      break;
    }

    server.send(415, "text/plain", "no hc22000 content in record");
    break;
  }

  f.close();
  if (!served && server.client().connected()) {
    server.send(404, "text/plain", "packet id not found");
  }
}

static void handleFriendsPage() {
  String html = String(HTML_LIST_TPL);
  html.replace("%TITLE%", "Friends");
  server.send(200, "text/html", html);
}

static void handlePacketsPage() {
  String html = String(HTML_LIST_TPL);
  html.replace("%TITLE%", "Packets");
  server.send(200, "text/html", html);
}

static void handleApiFriends() {
  server.send(200, "application/json", ndjsonToJsonArray(FR_TBL));
}

static void handleApiPackets() {
  server.send(200, "application/json", ndjsonToJsonArray(PK_TBL));
}

void initAPConfig() {
  server.on("/", handleRoot);
  server.on("/friends", HTTP_GET, handleFriendsPage);
  server.on("/packets", HTTP_GET, handlePacketsPage);

  server.on("/api/config", handleGetConfig);
  server.on("/api/save", HTTP_POST, handleSaveConfig);
  server.on("/api/reset", HTTP_POST, handleResetConfig);
  server.on("/api/friends", HTTP_GET, handleApiFriends);
  server.on("/api/packets", HTTP_GET, handleApiPackets);
  server.on("/api/packet/download", HTTP_GET, handleDownloadPacket);
}

static bool saneApCreds(const char* ssid, const char* pass) {
  size_t ls = ssid ? strlen(ssid) : 0;
  size_t lp = pass ? strlen(pass) : 0;
  if (ls == 0 || ls > 32) { Serial.println("AP SSID invalid length"); return false; }
  if (lp != 0 && (lp < 8 || lp > 63)) { Serial.println("AP password invalid length"); return false; }
  return true;
}

void startAPMode() {
  Serial.println("AP Mode - Starting...");
  if (ap_mode_active) return;

  // Stop tutto
  Serial.println("Stop promiscuous mode and station mode");
  esp_wifi_set_promiscuous(false);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);  // drop STA + forget creds
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(50);

  Serial.printf("Free heap before AP: %u\n", ESP.getFreeHeap());

  if (!saneApCreds(AP_SSID, AP_PASSWORD)) {
    Serial.println("ERROR: SSID/PASS not valid");
    return;
  }

  WiFi.mode(WIFI_AP);
  delay(50);

  // Opzionale: IP della SoftAP (se vuoi uno specifico)
  // WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  Serial.println("Start Soft Access Point");
  const int channel = 1;    // evita DFS e canali dubbi
  const bool hidden = false;
  const int max_conn = 4;

  bool ap_success = WiFi.softAP(AP_SSID, AP_PASSWORD, channel, hidden, max_conn);

  if (!ap_success) {
    Serial.println("Arduino softAP() returned false. Provo a ottenere l'errore basso livello...");

    // Prova con API native per loggare l'err (opzionale)
    wifi_config_t cfg{};
    strlcpy((char*)cfg.ap.ssid, AP_SSID, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(AP_SSID);
    if (AP_PASSWORD && strlen(AP_PASSWORD) >= 8) {
      strlcpy((char*)cfg.ap.password, AP_PASSWORD, sizeof(cfg.ap.password));
      cfg.ap.authmode = WIFI_AUTH_WPA2_PSK; // semplice & supportato
    } else {
      cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    cfg.ap.channel = channel;
    cfg.ap.max_connection = max_conn;

    esp_err_t e;
    e = esp_wifi_set_mode(WIFI_MODE_AP);
    if (e) Serial.printf("esp_wifi_set_mode: %s\n", esp_err_to_name(e));
    e = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (e) Serial.printf("esp_wifi_set_config: %s\n", esp_err_to_name(e));
    e = esp_wifi_start();
    if (e) {
      Serial.printf("[LOW] esp_wifi_start: %s\n", esp_err_to_name(e));
      Serial.println("ERROR: Failed to start AP mode!");
      ap_mode_active = false;
      return;
    } else {
      Serial.println("SoftAP started via native API.");
    }
  }

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: "); Serial.println(IP);

  server.begin();
  ap_mode_active = true;
  ap_start_time = millis();
  Serial.println("AP Mode - Started.");
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



