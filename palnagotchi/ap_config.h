#ifndef AP_CONFIG_H
#define AP_CONFIG_H

#include "Arduino.h"
#include "ArduinoJson.h"
#include "M5Unified.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "WebServer.h"
#include "LittleFS.h"
#include "mbedtls/base64.h"   // per decodificare base64 (ESP-IDF)
#include "config.h"

using namespace std;

#ifndef FR_TBL
#define FR_TBL "/friends.ndjson"
#endif

#ifndef PK_TBL
#define PK_TBL "/packets.ndjson"
#endif

static const char* HTML_LIST_TPL = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>%TITLE%</title>
<style>
body{font-family:system-ui,Arial,sans-serif;background:#111;color:#e6e6e6;margin:0;padding:20px}
h1{font-size:20px;margin:0 0 14px}
a{color:#5cc8ff;text-decoration:none}
.card{border:1px solid #2a2a2a;border-radius:10px;padding:12px;margin:10px 0;background:#1a1a1a}
small{color:#aaa}.badge{display:inline-block;border:1px solid #2a2a2a;border-radius:6px;padding:2px 6px;margin-left:6px;color:#bbb}
</style></head><body>
<h1>%TITLE%</h1>
<div id="list"></div>
<script>
async function load(url, render){
  const r=await fetch(url); const data=await r.json();
  const root=document.getElementById('list'); root.innerHTML='';
  if(!Array.isArray(data)||!data.length){root.innerHTML='<small>Empty.</small>';return;}
  data.forEach(render);
}
function esc(s){return String(s??'').replace(/[&<>"]/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[m]));}

if(location.pathname==='/friends'){
  load('/api/friends', (row)=>{
    const d=document.createElement('div'); d.className='card';
    const name=esc(row.name||'—'), id=esc(row.identity||'—');
    const ver=esc(row.version||''), rssi=(row.rssi!=null?(' rssi:'+row.rssi):'');
    d.innerHTML = `<div><b>${name}</b> <span class="badge">${id}</span></div>
                   <div><small>face:${esc(row.face)} grid_ver:${esc(row.grid_version)} ver:${ver}${rssi}</small></div>`;
    document.getElementById('list').appendChild(d);
  });
}

if(location.pathname==='/packets'){
  load('/api/packets', (row)=>{
    const d=document.createElement('div'); d.className='card';
    const id=esc(row.id||'—'), ssid=esc(row.ssid||''); const bssid=esc(row.bssid||'');
    const href=`/api/packet/download?id=${encodeURIComponent(id)}`;
    d.innerHTML = `<div><b>${ssid||'(ssid sconosciuto)'}</b>
                   <span class="badge">id:${id}</span>
                   <span class="badge">${bssid}</span></div>
                   <div><a href="${href}">Download .hc22000</a></div>`;
    document.getElementById('list').appendChild(d);
  });
}
</script></body></html>
)HTML";



// AP Configuration
#define AP_SSID "Gotchi"
#define AP_PASSWORD "GotchiPass"
#define AP_TIMEOUT_MS 300000  // 5 minutes

void initAPConfig();
void startAPMode();
void stopAPMode();
void handleAPConfig();
bool isAPModeActive();
bool shouldExitAPMode();

#endif
