#ifndef _STRUCTS_H_
#define _STRUCTS_H_

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
  double latitude;
  double longitude;
  bool has_gps;
} pwngrid_peer;

typedef struct {
  uint8_t data[800]; // to be adapted to max packet length to be saved
  size_t len;
  uint8_t channel;
  char bssid[18]; // "aa:bb:cc:dd:ee:ff"
  char type[16];  // "EAPOL" / "PMKID"
  int64_t ts_ms;
} packet_item_t;
#endif