#include "pwngrid.h"
#include "config.h"

unsigned long fullPacketStartTime = 0;
const unsigned long PACKET_TIMEOUT_MS = 5000; // 5 seconds

uint8_t pwngrid_friends_tot = 0;
uint8_t pwngrid_friends_run = 0;

pwngrid_peer pwngrid_peers[255];
String pwngrid_last_friend_name = "";

uint8_t pwngrid_pwned_tot;
uint8_t pwngrid_pwned_run;

String fullPacket = "";

DeviceConfig *config = getConfig();

String reverse(const char * original) {
  String reverse = "";
  int str_len = String(original).length();

  if(str_len > 0 && original != NULL)
  {
    for(int i = 0 ; i < str_len ; ++i)
    {
      reverse.concat(original[str_len - i - 2]);
    }

    //reverse[size - 1] = '\0';
  }
  return reverse;
}

// helper: format MAC -> string
void MAC2str(const uint8_t mac[6], char *out /*18 bytes*/) {
  sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void dbFriendTask(void *pv) {
  pwngrid_peer item;
  for (;;) {
    if (xQueueReceive(frQueue, &item, portMAX_DELAY) == pdTRUE) {
      if (!mergeFriend(item)) {
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

void enqueue_friend_from_sniffer(pwngrid_peer a_friend) {
  if (!frQueue) {
    Serial.println("enqueue_friend_from_sniffer error: frQueue is invalid");
    return;
  }

  BaseType_t ok = xQueueSend(frQueue, &a_friend, 0);       // no wait
  if (ok != pdTRUE) {
    Serial.println("Failed enqueuing friend");
    return;
  }
  //Serial.println("Friend added to queue: " + a_friend.name);
}

uint8_t getPwngridTotalPeers() {
  return pwngrid_friends_run;
}
uint8_t getPwngridRunTotalPeers() {
  return pwngrid_friends_tot;
}
String getPwngridLastFriendName() {
  return pwngrid_last_friend_name;
}
pwngrid_peer *getPwngridPeers() {
  return pwngrid_peers;
}
uint8_t getPwngridTotalPwned() {
  return EEPROM.read(1);
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

  pal_json["pal"] = true;  // Also detect other Gotchis
  pal_json["name"] = getDeviceName();
  pal_json["face"] = face;
  pal_json["epoch"] = 1;
  pal_json["grid_version"] = "1.10.3";
  pal_json["identity"] =
    "32e9f315e92d974342c93d0fd952a914bfb4e6838953536ea6f63d54db6b9610";
  pal_json["pwnd_run"] = 0;
  pal_json["pwnd_tot"] = 0;
  pal_json["session_id"] = "a2:00:64:e6:0b:8b";
  pal_json["timestamp"] = 0;
  pal_json["uptime"] = 0;
  pal_json["version"] = "1.8.4";
  pal_json["policy"]["advertise"] = true;
  pal_json["policy"]["bond_encounters_factor"] = 20000;
  pal_json["policy"]["bored_num_epochs"] = 0;
  pal_json["policy"]["sad_num_epochs"] = 0;
  pal_json["policy"]["excited_num_epochs"] = 9999;

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

  //Serial.println("pwngridAddPeer...");

  for (uint8_t i = 0; i < pwngrid_friends_tot; i++) {
    // Check if peer identity is already in peers array
    if (pwngrid_peers[i].identity == identity) {
      pwngrid_peers[i].last_ping = millis();
      pwngrid_peers[i].gone = false;
      pwngrid_peers[i].rssi = rssi;
      Serial.println("Peer already added");
      return;
    }
  }

  pwngrid_peers[pwngrid_friends_tot].rssi = rssi;
  pwngrid_peers[pwngrid_friends_tot].last_ping = millis();
  pwngrid_peers[pwngrid_friends_tot].gone = false;
  pwngrid_peers[pwngrid_friends_tot].name = json["name"].as<String>();
  pwngrid_peers[pwngrid_friends_tot].face = json["face"].as<String>();
  pwngrid_peers[pwngrid_friends_tot].epoch = json["epoch"].as<int>();
  pwngrid_peers[pwngrid_friends_tot].grid_version = json["grid_version"].as<String>();
  pwngrid_peers[pwngrid_friends_tot].identity = identity;
  pwngrid_peers[pwngrid_friends_tot].pwnd_run = json["pwnd_run"].as<int>();
  pwngrid_peers[pwngrid_friends_tot].pwnd_tot = json["pwnd_tot"].as<int>();
  pwngrid_peers[pwngrid_friends_tot].session_id = json["session_id"].as<String>();
  pwngrid_peers[pwngrid_friends_tot].timestamp = json["timestamp"].as<int>();
  pwngrid_peers[pwngrid_friends_tot].uptime = json["uptime"].as<int>();
  pwngrid_peers[pwngrid_friends_tot].version = json["version"].as<String>();
  pwngrid_peers[pwngrid_friends_tot].channel = channel;

  pwngrid_last_friend_name = pwngrid_peers[pwngrid_friends_tot].name;

  enqueue_friend_from_sniffer(pwngrid_peers[pwngrid_friends_tot]);

  EEPROM.write(0, pwngrid_friends_tot);
  EEPROM.commit();

  pwngrid_friends_run++;
  pwngrid_friends_tot++;
}

const int away_threshold = 120000;

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

// helper: stampa un buffer in hex
void printHex(const uint8_t *b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (b[i] < 0x10) Serial.print('0');
    Serial.print(b[i], HEX);
    if (i + 1 < len) Serial.print(':');
  }
  Serial.println();
}

void handlePacket(wifi_promiscuous_pkt_t *snifferPacket) {
  wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)snifferPacket;
  uint8_t *pkt = ppkt->payload;
  int pkt_len = ppkt->rx_ctrl.sig_len ? ppkt->rx_ctrl.sig_len : 300;  // usa la len reale se disponibile
  int rx_channel = ppkt->rx_ctrl.channel;

  bool isEapol = false;
  if (pkt_len > 34) {  // semplice bound check
    if ((pkt[30] == 0x88 && pkt[31] == 0x8e) || (pkt[32] == 0x88 && pkt[33] == 0x8e)) {
      isEapol = true;
    }
  }

  if (isEapol) {
    Serial.println("We have EAPOL");
    drawMood(pwnagotchi_moods[10], "I love EAPOLs!", false);
    pwngrid_pwned_run++;
    pwngrid_pwned_tot++;
    EEPROM.write(1, pwngrid_pwned_tot);
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
      // control frames: non gestiti
      memcpy(bssid, addr3, 6);  // fallback
    }
    enqueue_packet_from_sniffer(pkt, pkt_len, bssid, "EAPOL", (uint8_t)rx_channel);
  }

  // --- ricerca RSN IE (element ID 0x30) per PMKID ---
  // L'RSN IE può comparire nei management frames (beacon/probe/assoc/etc)
  // e la struttura interna ha campi variabili. Fare parsing difensivo.
  // Cerchiamo semplicemente il tag 0x30 (RSN) e poi scorriamo al suo interno
  // per trovare PMKID count + PMKID list (se presenti).
  //
  // Nota: evitare di leggere oltre il buffer: assumiamo che il pacchetto abbia almeno 64 byte,
  // ma facciamo controlli di lunghezza usando il length dichiarato se disponibile.
  size_t max_scan = 230;  // limite ragionevole per non scorrere troppo oltre
  // se la struttura wifi_promiscuous_pkt_t fornisce una lunghezza, sostituire max_scan con quella.
  // es.: size_t pkt_len = snifferPacket->rx_ctrl.sig_len; (dipende SDK)

  for (size_t i = 0; i + 1 < max_scan; ++i) {
    // cerca element ID 0x30 (RSN IE)
    if (pkt[i] == 0x30) {
      // assicurarsi di poter leggere la lunghezza dell'IE
      uint8_t ie_len = pkt[i + 1];
      size_t ie_start = i + 2;
      size_t ie_end = ie_start + ie_len;
      if (ie_end > max_scan) {
        // IE troncato o va oltre il buffer di scan: skip
        continue;
      }

      // Parsing difensivo dell'RSN IE:
      // struttura generale (semplificata):
      // Version (2) | GroupCipher(4) |
      // PairwiseCount(2) + PairwiseList(4*count) |
      // AKMCount(2) + AKMList(4*count) |
      // [RSNCapabilities(2)] |
      // [PMKIDCount(2) + PMKIDList(16*count)]
      size_t pos = ie_start;
      if (pos + 2 > ie_end) continue;  // versione
      pos += 2;                        // version

      // group cipher 4 bytes
      if (pos + 4 > ie_end) continue;
      pos += 4;

      // pairwise count (2 bytes)
      if (pos + 2 > ie_end) continue;
      uint16_t pairwise_count = pkt[pos] | (pkt[pos + 1] << 8);
      pos += 2;
      // pairwise list: 4 * count
      size_t pairwise_bytes = (size_t)pairwise_count * 4;
      if (pos + pairwise_bytes > ie_end) continue;
      pos += pairwise_bytes;

      // AKM count (2)
      if (pos + 2 > ie_end) continue;
      uint16_t akm_count = pkt[pos] | (pkt[pos + 1] << 8);
      pos += 2;
      // akm list: 4 * count
      size_t akm_bytes = (size_t)akm_count * 4;
      if (pos + akm_bytes > ie_end) continue;
      pos += akm_bytes;

      // after AKM, there **may** be RSN Capabilities (2 bytes)
      if (pos + 2 <= ie_end) {
        // peek but not mandatory to have
        // we will advance if space left
        // but we must ensure we don't step over in case PMKID not present
        // so check remaining length
        // compute remaining bytes
      }

      // compute remaining bytes in IE
      size_t remaining = (ie_end > pos) ? (ie_end - pos) : 0;
      if (remaining >= 2) {
        // possible RSN Capabilities present (2 bytes) + maybe PMKIDCount after
        // We'll try to detect PMKIDCount by checking if after RSN Capabilities there is at least 2 bytes for pmkid_count
        size_t pos_after_caps = pos;
        if (pos_after_caps + 2 <= ie_end) {
          // treat the next two as RSN Capabilities (if present)
          pos_after_caps += 2;
        }
        // check if we have PMKID count field now
        if (pos_after_caps + 2 <= ie_end) {
          uint16_t pmkid_count = pkt[pos_after_caps] | (pkt[pos_after_caps + 1] << 8);
          // sanity check: pmkid_count reasonable (e.g., 1 or small)
          if (pmkid_count > 0 && pmkid_count <= 8) {
            // ensure enough bytes for pmkid list (16 * count)
            size_t needed = (size_t)pmkid_count * 16;
            if (pos_after_caps + 2 + needed <= ie_end) {
              Serial.print("Found PMKID count: ");
              Serial.println(pmkid_count);
              // read each PMKID (16 bytes each)
              size_t pmkid_pos = pos_after_caps + 2;
              for (uint16_t k = 0; k < pmkid_count; ++k) {
                uint16_t fc = pkt[0] | (pkt[1] << 8);
                bool toDS = fc & 0x0100;
                bool fromDS = fc & 0x0200;
                uint8_t bssid[6];
                uint8_t fc_type = (fc >> 2) & 0x3;  // 0=mgmt,1=ctrl,2=data
                uint8_t addr1[6], addr2[6], addr3[6], addr4[6];
                memcpy(addr1, pkt + 4, 6);
                memcpy(addr2, pkt + 10, 6);
                memcpy(addr3, pkt + 16, 6);
                if (pkt_len >= 30) memcpy(addr4, pkt + 24, 6);

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
                  // control frames: non gestiti
                  memcpy(bssid, addr3, 6);  // fallback
                }

                Serial.print("PMKID #");
                Serial.print(k);
                Serial.print(": ");
                printHex(&pkt[pmkid_pos + k * 16], 16);
                enqueue_packet_from_sniffer(pkt, 16, bssid, "PMKID", (uint8_t)rx_channel);
              }
              drawMood(pwnagotchi_moods[10], "I love PMKIDs!", false);
              pwngrid_pwned_run++;
              pwngrid_pwned_tot++;
              EEPROM.write(1, pwngrid_pwned_tot);
              i = ie_end;  // salta avanti
              break;
            }
          }
        }
      }
    }  // fi if pkt[i] == 0x30
  }    // for scan
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
    if (config->personality == PASSIVE || config->personality == AGGRESSIVE) {
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
              // Start of NEW JSON
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
                  Serial.println("Complete JSON: " + fullPacket);
                  
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

void initPwngrid() {
  int rc;

  Serial.println("Init Pwngrid");

  initDB();
  initDBWorkers();

  // Disable WiFi logging
  esp_log_level_set("wifi", ESP_LOG_NONE);

  wifi_init_config_t WIFI_INIT_CONFIG = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&WIFI_INIT_CONFIG);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_start();
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&pwnSnifferCallback);
  // esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(random(0, 14), WIFI_SECOND_CHAN_NONE);
  delay(1);
  Serial.println("Pwngrid initialised.");
}
