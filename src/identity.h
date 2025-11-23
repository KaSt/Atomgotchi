#ifndef PWN_IDENTITY_H
#define PWN_IDENTITY_H

#include <Arduino.h>

struct PwnIdentity {
  String name;        // es. "brisk-otter"
  String device_mac;  // es. "84:0d:8e:aa:bb:cc"
  String priv_hex;    // 64 hex
  String pub_hex;     // 66 hex (compressed) 
  String short_id;    // sha256(pub or priv) first 10 hex
};

PwnIdentity ensurePwnIdentity(bool recompute_pub_if_missing = true);

static String getBaseMAC();
String getPublicPEM();
String getFingerprintHex();
String getSessionId();

#endif // PWN_IDENTITY_H
