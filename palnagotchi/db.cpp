#include "db.h"

const char* FR_TBL = "/friends.ndjson";
const char* PKT_TBL = "/packets.ndjson";  


void initDB() {
  if (!LittleFS.begin(true)) {
    Serial.println("Rebooting to see if the FS is now ok");
    ESP.restart();
  };
}

/*
typedef struct {
  int epoch;
  String face;
  String grid_version;
  String identity;
  String name;
  int pwnd_run;
  int pwnd_tot;
  String session_id;
  int timestamp;
  int uptime;
  String version;
  signed int rssi;
  int last_ping;
  bool gone;
  int channel;
} pwngrid_peer;
*/

bool addFriend(pwngrid_peer newFriend) {
  File f = LittleFS.open(FR_TBL, FILE_WRITE);
  if (!f) {
    Serial.println("addFriend: Error opening FR_TBL");
    return false;
  }
  StaticJsonDocument<256> friendJSON;
  friendJSON["epoch"] = newFriend.epoch;
  friendJSON["face"] = newFriend.face;
  friendJSON["grid_version"] = newFriend.grid_version;
  friendJSON["identity"] = newFriend.identity;
  friendJSON["name"] = newFriend.name;
  friendJSON["session_id"] = newFriend.session_id;
  friendJSON["timestamp"] = newFriend.timestamp;
  friendJSON["uptime"] = newFriend.uptime;
  friendJSON["version"] = newFriend.version;
  friendJSON["rssi"] = newFriend.rssi;
  friendJSON["last_ping"] = newFriend.last_ping;
  friendJSON["gone"] = newFriend.gone;
  friendJSON["channel"] = newFriend.channel;

  serializeJson(friendJSON, f);
  f.write('\n');
  f.flush();  
  f.close();
  Serial.println("Friend added to DB");
  return true;
}

// Assunzioni:
// - FR_TBL è un file NDJSON (una riga = un JSON) già definito altrove
// - pwngrid_peer ha i campi usati sotto (epoch, face, grid_version, identity, name, session_id, timestamp, uptime, version, rssi, last_ping, gone, channel)
// - LittleFS è già inizializzato
// - ArduinoJson incluso

bool mergeFriend(const pwngrid_peer &nf) {

  Serial.println("Checking Friend name: "+nf.name);

  File in = LittleFS.open(FR_TBL, FILE_READ);
  if (!in) {
    // DB non ancora creato: inseriamo semplicemente
    Serial.println("Not present in DB yet, calling addFriend for: " + nf.name);
    return addFriend(nf);
  }

  // File temporaneo per scrittura atomica
  String tmpPath = String(FR_TBL) + ".tmp";
  File out = LittleFS.open(tmpPath.c_str(), FILE_WRITE);
  if (!out) {
    Serial.println("mergeFriend: Error opening temp file");
    in.close();
    return false;
  }

  bool found = false;
  StaticJsonDocument<512> doc; // adegua se le righe sono più corpose

  while (in.available()) {
    String line = in.readStringUntil('\n');
    if (line.length() == 0) continue;

    doc.clear();
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
      out.print(line);
      out.write('\n');
      continue;
    }

    JsonObject obj = doc.as<JsonObject>();
    const char* id = obj["identity"] | "";

    if (id && nf.identity && String(id) == String(nf.identity)) {
      obj["face"]         = nf.face;
      obj["grid_version"] = nf.grid_version;
      obj["session_id"]   = nf.session_id;
      obj["uptime"]       = nf.uptime;
      obj["version"]      = nf.version;
      obj["rssi"]         = nf.rssi;
      obj["last_ping"]    = nf.last_ping;
      obj["gone"]         = nf.gone;
      obj["channel"]      = nf.channel;
      found = true;
    }

    serializeJson(obj, out);
    out.write('\n');
  }

  in.close();
  out.flush();
  out.close();

  if (!found) {
    // Add
    File out2 = LittleFS.open(tmpPath.c_str(), FILE_WRITE); // apre in append su ESP32
    if (!out2) {
      Serial.println("mergeFriend: Error reopening temp for append");
      LittleFS.remove(tmpPath.c_str());
      return false;
    }
    out2.seek(out2.size()); // assicurati di essere in append

    StaticJsonDocument<256> friendJSON;
    friendJSON["epoch"]        = nf.epoch;
    friendJSON["face"]         = nf.face;
    friendJSON["grid_version"] = nf.grid_version;
    friendJSON["identity"]     = nf.identity;
    friendJSON["name"]         = nf.name;
    friendJSON["session_id"]   = nf.session_id;
    friendJSON["timestamp"]    = nf.timestamp;
    friendJSON["uptime"]       = nf.uptime;
    friendJSON["version"]      = nf.version;
    friendJSON["rssi"]         = nf.rssi;
    friendJSON["last_ping"]    = nf.last_ping;
    friendJSON["gone"]         = nf.gone;
    friendJSON["channel"]      = nf.channel;

    serializeJson(friendJSON, out2);
    out2.write('\n');
    out2.flush();
    out2.close();
  }

  LittleFS.remove(FR_TBL);
  if (!LittleFS.rename(tmpPath.c_str(), FR_TBL)) {
    Serial.println("mergeFriend: Error renaming temp to FR_TBL");
    // Try to cleanup
    LittleFS.remove(tmpPath.c_str());
    return false;
  }

  Serial.println(found ? "Friend updated in DB" : "Friend inserted in DB");
  return true;
}


/*
typedef struct {
  uint8_t data[800]; // to be adapted to max packet length to be saved
  size_t len;
  uint8_t channel;
  char bssid[18]; // "aa:bb:cc:dd:ee:ff"
  char type[16];  // "EAPOL" / "PMKID"
  int64_t ts_ms;
} packet_item_t;
*/

bool addPacket(packet_item_t packet) {
  File f = LittleFS.open(PKT_TBL, FILE_WRITE);
  if (!f) {
    Serial.println("addPacket: Error opening PKT_TBL");
    return false;
  }
  StaticJsonDocument<256> pktJSON;
  pktJSON["data"] = packet.data;
  pktJSON["len"] = packet.len;
  pktJSON["channel"] = packet.channel;
  pktJSON["type"] = packet.type;
  pktJSON["ts_ms"] = packet.ts_ms;
  serializeJson(pktJSON, f);
  f.write('\n');
  f.flush();  
  f.close();
  Serial.println("Packet added to DB");
  return true;
}
