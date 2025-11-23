#include "db.h"
#include "storage.h"

const char* FR_TBL = "/friends.ndjson";
const char* PKT_TBL = "/packets.ndjson";  


void initDB() {
  // Storage manager handles LittleFS and SD initialization
  if (!storage.begin()) {
    Serial.println("Storage: Critical failure - rebooting");
    ESP.restart();
  }
  
  Serial.printf("DB: Using %s for storage\n", storage.getStorageTypeName());
}

bool addFriend(pwngrid_peer newFriend) {
  File f = storage.open(FR_TBL, FILE_WRITE);
  if (!f) {
    Serial.println("addFriend: Error opening FR_TBL");
    return false;
  }
  StaticJsonDocument<384> friendJSON;  // Increased for GPS data
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
  
  // Save GPS coordinates if available
  if (newFriend.has_gps) {
    friendJSON["latitude"] = newFriend.latitude;
    friendJSON["longitude"] = newFriend.longitude;
    friendJSON["has_gps"] = true;
  } else {
    friendJSON["has_gps"] = false;
  }

  serializeJson(friendJSON, f);
  f.write('\n');
  f.flush();  
  f.close();
  Serial.println("Friend added to DB" + String(newFriend.has_gps ? " (with GPS)" : ""));
  return true;
}

bool mergeFriend(const pwngrid_peer &nf, uint64_t &pwngrid_friends_tot) {

Serial.println(String("Checking Friend name: ") + nf.name);

  File in = storage.open(FR_TBL, FILE_READ);
  if (!in) {
    Serial.println("Not present in DB yet, calling addFriend for: " + nf.name);
    pwngrid_friends_tot++;
    Serial.println("We have now met a total friends of: ");
    Serial.println(pwngrid_friends_tot);
    return addFriend(nf);
  }

  String tmpPath = String(FR_TBL) + ".tmp";
  File out = storage.open(tmpPath.c_str(), FILE_WRITE);
  if (!out) {
    Serial.println("mergeFriend: Error opening temp file");
    in.close();
    return false;
  }

  bool found = false;
  StaticJsonDocument<512> doc; // Change according to size

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
    pwngrid_friends_tot++;

    File out2 = storage.open(tmpPath.c_str(), FILE_APPEND);
    if (!out2) {
      Serial.println("mergeFriend: Error reopening temp for append");
      storage.remove(tmpPath.c_str());
      return false;
    }
    out2.seek(out2.size()); 

    StaticJsonDocument<384> friendJSON;  // Increased for GPS data
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
    
    // Save GPS coordinates if available
    if (nf.has_gps) {
      friendJSON["latitude"]   = nf.latitude;
      friendJSON["longitude"]  = nf.longitude;
      friendJSON["has_gps"]    = true;
    } else {
      friendJSON["has_gps"]    = false;
    }

    serializeJson(friendJSON, out2);
    out2.write('\n');
    out2.flush();
    out2.close();
  }

  storage.remove(FR_TBL);
  if (!storage.rename(tmpPath.c_str(), FR_TBL)) {
    Serial.println("mergeFriend: Error renaming temp to FR_TBL");
    // Try to cleanup
    storage.remove(tmpPath.c_str());
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
  File f = storage.open(PKT_TBL, FILE_WRITE);
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
