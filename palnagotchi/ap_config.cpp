#include "ap_config.h"

WebServer server(80);
bool ap_mode_active = false;
unsigned long ap_start_time = 0;

// JSON parsing constants
const char JSON_DEVICE_NAME_KEY[] = "\"device_name\":\"";
const char JSON_BRIGHTNESS_KEY[] = "\"brightness\":";
const char JSON_PERSONALITY_KEY[] = "\"personality\":\"";
const size_t JSON_DEVICE_NAME_KEY_LEN = sizeof(JSON_DEVICE_NAME_KEY) - 1;
const size_t JSON_BRIGHTNESS_KEY_LEN = sizeof(JSON_BRIGHTNESS_KEY) - 1;
const size_t JSON_PERSONALITY_KEY_LEN = sizeof(JSON_PERSONALITY_KEY) - 1;

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Gotchi Configuration</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body { 
      font-family: 'Courier New', monospace;
      background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);
      color: #00ff41;
      min-height: 100vh;
      padding: 20px;
    }
    
    .container {
      max-width: 600px;
      margin: 0 auto;
    }
    
    header {
      text-align: center;
      margin-bottom: 30px;
      padding: 20px;
      background: rgba(0, 255, 65, 0.05);
      border: 2px solid #00ff41;
      border-radius: 10px;
      position: relative;
      overflow: hidden;
    }
    
    header::before {
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 100%;
      background: linear-gradient(90deg, transparent, rgba(0, 255, 65, 0.1), transparent);
      animation: scan 3s infinite;
    }
    
    @keyframes scan {
      0% { left: -100%; }
      100% { left: 100%; }
    }
    
    h1 { 
      font-size: 2em;
      color: #00ff41;
      text-shadow: 0 0 10px #00ff41, 0 0 20px #00ff41;
      margin-bottom: 10px;
      letter-spacing: 2px;
    }
    
    .ascii-art {
      font-size: 0.6em;
      line-height: 1.2;
      color: #00cc33;
      margin-top: 10px;
    }
    
    nav {
      display: flex;
      justify-content: center;
      gap: 20px;
      margin-bottom: 30px;
      flex-wrap: wrap;
    }
    
    nav a {
      color: #00ff41;
      text-decoration: none;
      padding: 10px 20px;
      border: 1px solid #00ff41;
      border-radius: 5px;
      transition: all 0.3s;
      background: rgba(0, 255, 65, 0.05);
    }
    
    nav a:hover {
      background: rgba(0, 255, 65, 0.2);
      box-shadow: 0 0 15px rgba(0, 255, 65, 0.5);
      transform: translateY(-2px);
    }
    
    .card {
      background: rgba(10, 10, 10, 0.8);
      border: 2px solid #00ff41;
      border-radius: 10px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 0 20px rgba(0, 255, 65, 0.2);
    }
    
    .form-group { 
      margin: 20px 0;
    }
    
    label { 
      display: block;
      margin-bottom: 8px;
      font-weight: bold;
      color: #00ff41;
      font-size: 0.95em;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    
    input[type="text"], input[type="number"] { 
      width: 100%;
      padding: 12px;
      font-size: 16px;
      border: 2px solid #00ff41;
      background: rgba(0, 0, 0, 0.5);
      color: #00ff41;
      border-radius: 5px;
      font-family: 'Courier New', monospace;
      transition: all 0.3s;
    }
    
    input[type="text"]:focus, input[type="number"]:focus {
      outline: none;
      box-shadow: 0 0 15px rgba(0, 255, 65, 0.5);
      background: rgba(0, 0, 0, 0.7);
    }
    
    .radio-group {
      display: flex;
      flex-direction: column;
      gap: 12px;
      margin-top: 10px;
    }
    
    .radio-option {
      display: flex;
      align-items: center;
      padding: 12px;
      border: 1px solid rgba(0, 255, 65, 0.3);
      border-radius: 5px;
      cursor: pointer;
      transition: all 0.3s;
      background: rgba(0, 0, 0, 0.3);
    }
    
    .radio-option:hover {
      background: rgba(0, 255, 65, 0.1);
      border-color: #00ff41;
    }
    
    .radio-option input[type="radio"] {
      margin-right: 12px;
      width: 18px;
      height: 18px;
      cursor: pointer;
      accent-color: #00ff41;
    }
    
    .radio-option label {
      margin: 0;
      cursor: pointer;
      flex: 1;
      text-transform: none;
      letter-spacing: normal;
      font-weight: normal;
    }
    
    button { 
      width: 100%;
      padding: 14px;
      font-size: 16px;
      background: #00ff41;
      color: #000;
      border: none;
      cursor: pointer;
      font-weight: bold;
      margin-top: 10px;
      border-radius: 5px;
      font-family: 'Courier New', monospace;
      text-transform: uppercase;
      letter-spacing: 2px;
      transition: all 0.3s;
      position: relative;
      overflow: hidden;
    }
    
    button::before {
      content: '';
      position: absolute;
      top: 50%;
      left: 50%;
      width: 0;
      height: 0;
      border-radius: 50%;
      background: rgba(255, 255, 255, 0.5);
      transform: translate(-50%, -50%);
      transition: width 0.6s, height 0.6s;
    }
    
    button:hover::before {
      width: 300px;
      height: 300px;
    }
    
    button:hover {
      box-shadow: 0 0 25px rgba(0, 255, 65, 0.8);
      transform: translateY(-2px);
    }
    
    button:active {
      transform: translateY(0);
    }
    
    button.secondary {
      background: transparent;
      color: #00ff41;
      border: 2px solid #00ff41;
    }
    
    button.secondary:hover {
      background: rgba(0, 255, 65, 0.1);
    }
    
    .message {
      padding: 15px;
      margin: 15px 0;
      border: 2px solid #00ff41;
      border-radius: 5px;
      text-align: center;
      background: rgba(0, 255, 65, 0.1);
      animation: slideDown 0.3s ease-out;
    }
    
    @keyframes slideDown {
      from {
        opacity: 0;
        transform: translateY(-20px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
    
    .info-box {
      background: rgba(0, 255, 65, 0.05);
      border-left: 4px solid #00ff41;
      padding: 12px;
      margin: 15px 0;
      font-size: 0.9em;
      border-radius: 3px;
    }
    
    .brightness-value {
      display: inline-block;
      min-width: 40px;
      text-align: center;
      color: #00ff41;
      font-weight: bold;
    }
    
    @media (max-width: 600px) {
      body {
        padding: 10px;
      }
      
      h1 {
        font-size: 1.5em;
      }
      
      .card {
        padding: 15px;
      }
      
      nav {
        gap: 10px;
      }
      
      nav a {
        padding: 8px 15px;
        font-size: 0.9em;
      }
    }
    
    .loading {
      display: inline-block;
      width: 20px;
      height: 20px;
      border: 3px solid rgba(0, 255, 65, 0.3);
      border-radius: 50%;
      border-top-color: #00ff41;
      animation: spin 1s linear infinite;
    }
    
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>⚡ GOTCHI CONFIG ⚡</h1>
      <div class="ascii-art">
        ( ◕‿◕) pwning the airwaves
      </div>
    </header>
    
    <nav>
      <a href="/">⚙️ Config</a>
      <a href="/friends">👥 Friends</a>
      <a href="/packets">📦 Packets</a>
    </nav>
    
    <div class="card">
      <form id="configForm">
        <div class="form-group">
          <label for="device_name">Device Name</label>
          <input type="text" id="device_name" name="device_name" maxlength="31" required placeholder="Enter device name">
          <div class="info-box" style="margin-top: 8px;">
            Max 31 characters. This identifies your device on the network.
          </div>
        </div>
        
        <div class="form-group">
          <label for="brightness">
            Display Brightness: <span class="brightness-value" id="brightness-display">128</span>
          </label>
          <input type="range" id="brightness" name="brightness" min="0" max="255" value="128" 
                 style="width: 100%; accent-color: #00ff41; height: 8px; cursor: pointer;">
          <div class="info-box" style="margin-top: 8px;">
            Range: 0 (off) to 255 (max). Adjust for battery life vs visibility.
          </div>
        </div>
        
        <div class="form-group">
          <label>Personality Mode</label>
          <div class="radio-group">
            <div class="radio-option">
              <input type="radio" id="friendly" name="personality" value="friendly" checked>
              <label for="friendly">🤝 Friendly - Just looking for friends</label>
            </div>
            <div class="radio-option">
              <input type="radio" id="passive" name="personality" value="passive">
              <label for="passive">👀 Passive - Quietly sniffing around</label>
            </div>
            <div class="radio-option">
              <input type="radio" id="aggressive" name="personality" value="aggressive">
              <label for="aggressive">💥 Aggressive - Actively hunting WiFi</label>
            </div>
          </div>
        </div>
        
        <button type="submit">💾 Save Configuration</button>
        <button type="button" class="secondary" onclick="resetConfig()">🔄 Reset to Defaults</button>
      </form>
    </div>
    
    <div id="message" class="message" style="display:none;"></div>
  </div>
  
  <script>
    // Update brightness display value
    const brightnessInput = document.getElementById('brightness');
    const brightnessDisplay = document.getElementById('brightness-display');
    
    brightnessInput.addEventListener('input', function() {
      brightnessDisplay.textContent = this.value;
    });
    
    // Load current configuration on page load
    window.onload = function() {
      const msg = document.getElementById('message');
      msg.textContent = 'Loading configuration...';
      msg.style.display = 'block';
      
      fetch('/api/config')
        .then(response => {
          if (!response.ok) throw new Error('Failed to load config');
          return response.json();
        })
        .then(data => {
          document.getElementById('device_name').value = data.device_name || '';
          
          const brightness = parseInt(data.brightness) || 128;
          document.getElementById('brightness').value = brightness;
          brightnessDisplay.textContent = brightness;
          
          const personality = (data.personality || 'friendly').toLowerCase();
          const radioBtn = document.getElementById(personality);
          if (radioBtn) radioBtn.checked = true;
          
          msg.textContent = '✓ Configuration loaded';
          setTimeout(() => { msg.style.display = 'none'; }, 2000);
        })
        .catch(error => {
          console.error('Error:', error);
          msg.textContent = '⚠ Failed to load configuration';
          setTimeout(() => { msg.style.display = 'none'; }, 3000);
        });
    };
    
    // Handle form submission
    document.getElementById('configForm').onsubmit = function(e) {
      e.preventDefault();
      
      const submitBtn = this.querySelector('button[type="submit"]');
      const originalText = submitBtn.innerHTML;
      submitBtn.innerHTML = '<span class="loading"></span> Saving...';
      submitBtn.disabled = true;
      
      const personality = document.querySelector('input[name="personality"]:checked').value;
      
      const data = {
        device_name: document.getElementById('device_name').value.trim(),
        brightness: parseInt(document.getElementById('brightness').value),
        personality: personality
      };
      
      fetch('/api/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      })
      .then(response => {
        if (!response.ok) throw new Error('Save failed');
        return response.json();
      })
      .then(result => {
        const msg = document.getElementById('message');
        msg.textContent = '✓ ' + (result.message || 'Configuration saved!');
        msg.style.display = 'block';
        msg.style.borderColor = '#00ff41';
        
        submitBtn.innerHTML = originalText;
        submitBtn.disabled = false;
        
        setTimeout(() => { msg.style.display = 'none'; }, 3000);
      })
      .catch(error => {
        console.error('Error:', error);
        const msg = document.getElementById('message');
        msg.textContent = '⚠ Failed to save configuration';
        msg.style.borderColor = '#ff4136';
        msg.style.display = 'block';
        
        submitBtn.innerHTML = originalText;
        submitBtn.disabled = false;
        
        setTimeout(() => { msg.style.display = 'none'; }, 3000);
      });
    };
    
    function resetConfig() {
      if (!confirm('⚠ Reset all settings to defaults?\n\nThis action cannot be undone.')) {
        return;
      }
      
      const msg = document.getElementById('message');
      msg.textContent = 'Resetting configuration...';
      msg.style.display = 'block';
      
      fetch('/api/reset', { method: 'POST' })
        .then(response => {
          if (!response.ok) throw new Error('Reset failed');
          return response.json();
        })
        .then(result => {
          msg.textContent = '✓ ' + (result.message || 'Configuration reset!');
          msg.style.borderColor = '#00ff41';
          setTimeout(() => { 
            location.reload(); 
          }, 1500);
        })
        .catch(error => {
          console.error('Error:', error);
          msg.textContent = '⚠ Failed to reset configuration';
          msg.style.borderColor = '#ff4136';
          setTimeout(() => { msg.style.display = 'none'; }, 3000);
        });
    }
  </script>
</body>
</html>
)rawliteral";

static const char* HTML_LIST_TPL = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>%TITLE% - Gotchi</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Courier New', monospace;
      background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);
      color: #00ff41;
      min-height: 100vh;
      padding: 20px;
    }
    
    .container {
      max-width: 900px;
      margin: 0 auto;
    }
    
    header {
      text-align: center;
      margin-bottom: 25px;
      padding: 20px;
      background: rgba(0, 255, 65, 0.05);
      border: 2px solid #00ff41;
      border-radius: 10px;
    }
    
    h1 {
      font-size: 1.8em;
      color: #00ff41;
      text-shadow: 0 0 10px #00ff41;
      letter-spacing: 2px;
    }
    
    nav {
      display: flex;
      justify-content: center;
      gap: 15px;
      margin-bottom: 25px;
      flex-wrap: wrap;
    }
    
    nav a {
      color: #00ff41;
      text-decoration: none;
      padding: 10px 20px;
      border: 1px solid #00ff41;
      border-radius: 5px;
      transition: all 0.3s;
      background: rgba(0, 255, 65, 0.05);
    }
    
    nav a:hover {
      background: rgba(0, 255, 65, 0.2);
      box-shadow: 0 0 15px rgba(0, 255, 65, 0.5);
      transform: translateY(-2px);
    }
    
    .card {
      border: 2px solid rgba(0, 255, 65, 0.3);
      border-radius: 8px;
      padding: 18px;
      margin: 12px 0;
      background: rgba(10, 10, 10, 0.7);
      transition: all 0.3s;
      animation: fadeIn 0.3s ease-out;
    }
    
    @keyframes fadeIn {
      from {
        opacity: 0;
        transform: translateY(10px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
    
    .card:hover {
      border-color: #00ff41;
      background: rgba(0, 255, 65, 0.05);
      box-shadow: 0 0 20px rgba(0, 255, 65, 0.2);
    }
    
    .card-header {
      display: flex;
      align-items: center;
      flex-wrap: wrap;
      gap: 10px;
      margin-bottom: 10px;
    }
    
    .card-title {
      font-size: 1.1em;
      font-weight: bold;
      color: #00ff41;
    }
    
    .badge {
      display: inline-block;
      border: 1px solid rgba(0, 255, 65, 0.5);
      border-radius: 4px;
      padding: 4px 10px;
      font-size: 0.85em;
      color: #00cc33;
      background: rgba(0, 255, 65, 0.1);
    }
    
    .card-details {
      color: #00cc33;
      font-size: 0.9em;
      line-height: 1.6;
    }
    
    .download-link {
      display: inline-block;
      margin-top: 10px;
      padding: 8px 16px;
      background: rgba(0, 255, 65, 0.1);
      color: #00ff41;
      text-decoration: none;
      border: 1px solid #00ff41;
      border-radius: 5px;
      transition: all 0.3s;
    }
    
    .download-link:hover {
      background: rgba(0, 255, 65, 0.2);
      box-shadow: 0 0 15px rgba(0, 255, 65, 0.5);
    }
    
    .empty-state {
      text-align: center;
      padding: 40px 20px;
      color: #00cc33;
      font-size: 1.1em;
      border: 2px dashed rgba(0, 255, 65, 0.3);
      border-radius: 10px;
      background: rgba(0, 255, 65, 0.02);
    }
    
    .loading {
      text-align: center;
      padding: 40px;
      color: #00ff41;
    }
    
    .loading::after {
      content: '...';
      animation: dots 1.5s steps(4, end) infinite;
    }
    
    @keyframes dots {
      0%, 20% { content: '.'; }
      40% { content: '..'; }
      60%, 100% { content: '...'; }
    }
    
    @media (max-width: 600px) {
      body {
        padding: 10px;
      }
      
      h1 {
        font-size: 1.4em;
      }
      
      .card {
        padding: 12px;
      }
      
      .badge {
        font-size: 0.75em;
        padding: 3px 8px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>⚡ %TITLE% ⚡</h1>
    </header>
    
    <nav>
      <a href="/">⚙️ Config</a>
      <a href="/friends">👥 Friends</a>
      <a href="/packets">📦 Packets</a>
    </nav>
    
    <div id="list" class="loading">Loading %TITLE%</div>
  </div>
  
  <script>
    async function load(url, render) {
      const listEl = document.getElementById('list');
      
      try {
        const r = await fetch(url);
        if (!r.ok) throw new Error('Failed to fetch');
        
        const data = await r.json();
        listEl.innerHTML = '';
        
        if (!Array.isArray(data) || !data.length) {
          listEl.innerHTML = '<div class="empty-state">📭 No data available yet.<br><small style="font-size:0.9em; margin-top:10px; display:block;">Start hunting and come back later!</small></div>';
          return;
        }
        
        data.forEach((item, index) => {
          setTimeout(() => render(item), index * 50);
        });
      } catch (error) {
        console.error('Error:', error);
        listEl.innerHTML = '<div class="empty-state" style="border-color: #ff4136; color: #ff4136;">⚠️ Failed to load data.<br><small>Check console for details.</small></div>';
      }
    }
    
    function esc(s) {
      return String(s ?? '').replace(/[&<>"']/g, m => ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#39;'
      }[m]));
    }
    
    if (location.pathname === '/friends') {
      load('/api/friends', (row) => {
        const d = document.createElement('div');
        d.className = 'card';
        
        const name = esc(row.name || 'Unknown');
        const id = esc(row.identity || 'N/A').substring(0, 16) + '...';
        const face = esc(row.face || '(◕‿◕)');
        const gridVer = esc(row.grid_version || 'N/A');
        const ver = esc(row.version || 'N/A');
        const rssi = row.rssi != null ? ` • Signal: ${row.rssi}dBm` : '';
        const channel = row.channel != null ? ` • Ch: ${row.channel}` : '';
        
        d.innerHTML = `
          <div class="card-header">
            <span class="card-title">${face} ${name}</span>
            <span class="badge">ID: ${id}</span>
          </div>
          <div class="card-details">
            Grid: ${gridVer} • Version: ${ver}${rssi}${channel}
          </div>
        `;
        
        document.getElementById('list').appendChild(d);
      });
    }
    
    if (location.pathname === '/packets') {
      load('/api/packets', (row) => {
        const d = document.createElement('div');
        d.className = 'card';
        
        const id = esc(row.id || 'N/A');
        const ssid = esc(row.ssid || '(hidden network)');
        const bssid = esc(row.bssid || 'N/A');
        const href = `/api/packet/download?id=${encodeURIComponent(id)}`;
        
        d.innerHTML = `
          <div class="card-header">
            <span class="card-title">📡 ${ssid}</span>
            <span class="badge">ID: ${id}</span>
          </div>
          <div class="card-details">
            BSSID: ${bssid}
          </div>
          <a href="${href}" class="download-link">📥 Download .hc22000</a>
        `;
        
        document.getElementById('list').appendChild(d);
      });
    }
  </script>
</body>
</html>
)HTML";

static String ndjsonToJsonArray(const char* path, size_t limit = 0) {
  File f = LittleFS.open(path, FILE_READ);
  String out = "[";
  if (!f) { out += "]"; return out; }

  StaticJsonDocument<1024> doc;
  bool first = true;
  size_t count = 0;
  
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    
    doc.clear();
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
      Serial.printf("JSON parse error in NDJSON: %s\n", err.c_str());
      continue;
    }

    if (!first) out += ",";
    first = false;
    
    String tmp;
    serializeJson(doc, tmp);
    out += tmp;
    
    count++;
    if (limit && count >= limit) break;
  }
  f.close();
  out += "]";
  
  return out;
}

void handleRoot() {
  server.send(200, "text/html", html_page);
}

void handleGetConfig() {
  DeviceConfig* config = getConfig();
  
  // Map personality enum to string
  String personalityStr = "friendly";
  if (config->personality == PASSIVE) {
    personalityStr = "passive";
  } else if (config->personality == AGGRESSIVE) {
    personalityStr = "aggressive";
  }
  
  String json = "{";
  json += "\"device_name\":\"" + String(config->device_name) + "\",";
  json += "\"brightness\":" + String(config->brightness) + ",";
  json += "\"personality\":\"" + personalityStr + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

int findJsonFieldEnd(const String& body, int start_pos) {
  int end_pos = body.indexOf(",", start_pos);
  if (end_pos == -1) {
    end_pos = body.indexOf("}", start_pos);
  }
  return end_pos;
}

void handleSaveConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"message\":\"Invalid request - no body\"}");
    return;
  }
  
  String body = server.arg("plain");
  DeviceConfig* config = getConfig();
  
  // Parse device_name
  int name_start = body.indexOf(JSON_DEVICE_NAME_KEY);
  if (name_start >= 0) {
    name_start += JSON_DEVICE_NAME_KEY_LEN;
    int name_end = body.indexOf("\"", name_start);
    if (name_end > name_start) {
      String name = body.substring(name_start, name_end);
      name.trim();
      if (name.length() > 0 && name.length() <= 31) {
        strncpy(config->device_name, name.c_str(), sizeof(config->device_name) - 1);
        config->device_name[sizeof(config->device_name) - 1] = '\0';
      }
    }
  }
  
  // Parse brightness
  int bright_start = body.indexOf(JSON_BRIGHTNESS_KEY);
  if (bright_start >= 0) {
    bright_start += JSON_BRIGHTNESS_KEY_LEN;
    int bright_end = findJsonFieldEnd(body, bright_start);
    if (bright_end > bright_start) {
      String bright = body.substring(bright_start, bright_end);
      bright.trim();
      int brightness_value = bright.toInt();
      if (brightness_value >= 0 && brightness_value <= 255) {
        config->brightness = brightness_value;
      }
    }
  }
  
  // Parse personality (now with quotes in the key)
  int personality_start = body.indexOf(JSON_PERSONALITY_KEY);
  if (personality_start >= 0) {
    personality_start += JSON_PERSONALITY_KEY_LEN;
    int personality_end = body.indexOf("\"", personality_start);
    if (personality_end > personality_start) {
      String personality_value = body.substring(personality_start, personality_end);
      personality_value.trim();
      personality_value.toLowerCase();
      
      if (personality_value == "friendly") {
        config->personality = FRIENDLY;
      } else if (personality_value == "passive") {
        config->personality = PASSIVE;
      } else if (personality_value == "aggressive") {
        config->personality = AGGRESSIVE;
      } else {
        config->personality = FRIENDLY;
      }
    }
  }
  
  saveConfig();
  
  server.send(200, "application/json", "{\"message\":\"Configuration saved! Changes will apply on restart.\"}");
}

void handleResetConfig() {
  resetConfig();
  saveConfig();
  server.send(200, "application/json", "{\"message\":\"Configuration reset to defaults!\"}");
}

static bool sendBytesAsFile(const String& filename, const uint8_t* data, size_t len) {
  WiFiClient client = server.client();
  if (!client.connected()) return false;
  
  String hdr = "HTTP/1.1 200 OK\r\n";
  hdr += "Content-Type: application/octet-stream\r\n";
  hdr += "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";
  hdr += "Content-Length: " + String(len) + "\r\n";
  hdr += "Connection: close\r\n\r\n";
  
  client.print(hdr);
  size_t sent = client.write(data, len);
  return sent == len;
}

static void handleDownloadPacket() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing id parameter");
    return;
  }
  
  const String wantId = server.arg("id");
  File f = LittleFS.open(PK_TBL, FILE_READ);
  
  if (!f) {
    server.send(404, "text/plain", "Packets table not found");
    return;
  }

  StaticJsonDocument<4096> doc;
  bool found = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;
    
    doc.clear();
    if (deserializeJson(doc, line)) continue;

    // Extract ID as string
    String idStr;
    if (doc["id"].is<const char*>()) {
      idStr = String(doc["id"].as<const char*>());
    } else if (doc["id"].is<int>()) {
      idStr = String(doc["id"].as<int>());
    } else if (doc["id"].is<unsigned int>()) {
      idStr = String(doc["id"].as<unsigned int>());
    }

    if (idStr != wantId) continue;
    found = true;

    // Case 1: Inline base64 content
    if (doc.containsKey("hc22000_b64")) {
      String b64 = doc["hc22000_b64"].as<String>();
      size_t needed = 0;
      
      int rc = mbedtls_base64_decode(nullptr, 0, &needed,
                                     (const unsigned char*)b64.c_str(), b64.length());
      
      if (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && rc != 0) {
        server.send(500, "text/plain", "Base64 length calculation error");
        break;
      }
      
      std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[needed]);
      if (!buf) {
        server.send(500, "text/plain", "Out of memory");
        break;
      }

      size_t outLen = 0;
      rc = mbedtls_base64_decode(buf.get(), needed, &outLen,
                                 (const unsigned char*)b64.c_str(), b64.length());
      
      if (rc != 0) {
        server.send(500, "text/plain", "Base64 decode error");
        break;
      }

      const String fname = "packet_" + idStr + ".hc22000";
      sendBytesAsFile(fname, buf.get(), outLen);
      break;
    }

    // Case 2: File path on LittleFS
    if (doc.containsKey("file")) {
      String p = doc["file"].as<String>();
      
      if (!LittleFS.exists(p)) {
        server.send(404, "text/plain", "File not found on filesystem");
        break;
      }

      File pf = LittleFS.open(p, FILE_READ);
      if (!pf) {
        server.send(500, "text/plain", "Cannot open file");
        break;
      }

      WiFiClient client = server.client();
      const String fname = p.endsWith(".hc22000") ? 
                          p.substring(p.lastIndexOf('/') + 1) : 
                          "packet_" + idStr + ".hc22000";
      
      String hdr = "HTTP/1.1 200 OK\r\n";
      hdr += "Content-Type: application/octet-stream\r\n";
      hdr += "Content-Disposition: attachment; filename=\"" + fname + "\"\r\n";
      hdr += "Content-Length: " + String(pf.size()) + "\r\n";
      hdr += "Connection: close\r\n\r\n";
      client.print(hdr);

      uint8_t buf[512];
      while (pf.available()) {
        size_t r = pf.read(buf, sizeof(buf));
        if (!r) break;
        client.write(buf, r);
      }
      pf.close();
      break;
    }

    server.send(415, "text/plain", "No hc22000 content in record");
    break;
  }

  f.close();
  
  if (!found && server.client().connected()) {
    server.send(404, "text/plain", "Packet ID not found");
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
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/save", HTTP_POST, handleSaveConfig);
  server.on("/api/reset", HTTP_POST, handleResetConfig);
  server.on("/api/friends", HTTP_GET, handleApiFriends);
  server.on("/api/packets", HTTP_GET, handleApiPackets);
  server.on("/api/packet/download", HTTP_GET, handleDownloadPacket);
}

static bool saneApCreds(const char* ssid, const char* pass) {
  size_t ls = ssid ? strlen(ssid) : 0;
  size_t lp = pass ? strlen(pass) : 0;
  
  if (ls == 0 || ls > 32) {
    Serial.println("AP SSID invalid length");
    return false;
  }
  
  if (lp != 0 && (lp < 8 || lp > 63)) {
    Serial.println("AP password invalid length");
    return false;
  }
  
  return true;
}

void startAPMode() {
  Serial.println("=== Starting AP Mode ===");
  if (ap_mode_active) {
    Serial.println("AP already active");
    return;
  }

  // Stop all WiFi activities
  Serial.println("Stopping promiscuous mode and station mode...");
  esp_wifi_set_promiscuous(false);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);

  Serial.printf("Free heap before AP: %u bytes\n", ESP.getFreeHeap());

  if (!saneApCreds(AP_SSID, AP_PASSWORD)) {
    Serial.println("ERROR: Invalid SSID/PASSWORD");
    return;
  }

  WiFi.mode(WIFI_AP);
  delay(100);

  Serial.println("Starting Soft Access Point...");
  const int channel = 1;
  const bool hidden = false;
  const int max_conn = 4;

  bool ap_success = WiFi.softAP(AP_SSID, AP_PASSWORD, channel, hidden, max_conn);

  if (!ap_success) {
    Serial.println("Arduino softAP() failed. Trying low-level API...");

    wifi_config_t cfg{};
    strlcpy((char*)cfg.ap.ssid, AP_SSID, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(AP_SSID);
    
    if (AP_PASSWORD && strlen(AP_PASSWORD) >= 8) {
      strlcpy((char*)cfg.ap.password, AP_PASSWORD, sizeof(cfg.ap.password));
      cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
      cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    cfg.ap.channel = channel;
    cfg.ap.max_connection = max_conn;

    esp_err_t e;
    e = esp_wifi_set_mode(WIFI_MODE_AP);
    if (e) Serial.printf("esp_wifi_set_mode error: %s\n", esp_err_to_name(e));
    
    e = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (e) Serial.printf("esp_wifi_set_config error: %s\n", esp_err_to_name(e));
    
    e = esp_wifi_start();
    if (e) {
      Serial.printf("esp_wifi_start error: %s\n", esp_err_to_name(e));
      Serial.println("ERROR: Failed to start AP mode!");
      return;
    }
    
    Serial.println("SoftAP started via native API");
  }

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.printf("SSID: %s\n", AP_SSID);
  Serial.printf("Connect and navigate to: http://%s\n", IP.toString().c_str());

  server.begin();
  ap_mode_active = true;
  ap_start_time = millis();
  
  Serial.println("=== AP Mode Started Successfully ===");
}

void stopAPMode() {
  if (!ap_mode_active) return;
  
  Serial.println("=== Stopping AP Mode ===");
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  ap_mode_active = false;
  Serial.println("AP Mode stopped");
}

void handleAPConfig() {
  if (!ap_mode_active) return;
  
  server.handleClient();
  
  // Auto-timeout
  if (millis() - ap_start_time > AP_TIMEOUT_MS) {
    Serial.println("AP timeout reached, stopping...");
    stopAPMode();
  }
}

bool isAPModeActive() {
  return ap_mode_active;
}

bool shouldExitAPMode() {
  return !ap_mode_active;
}