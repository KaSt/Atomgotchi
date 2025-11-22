#include "identity.h"
#include <Preferences.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <memory>

#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pem.h>

static const char* NVS_NS = "pwn-ident";

// Nickname bits (same UX you had)
static const char* ADJ[] PROGMEM = {
  "brisk","calm","cheeky","clever","daring","eager","fuzzy","gentle","merry","nimble",
  "peppy","quirky","sly","spry","steady","swift","tidy","witty","zesty","zen"
};
static const char* ANM[] PROGMEM = {
  "otter","badger","fox","lynx","marten","panda","yak","koala","civet","ibis",
  "gecko","lemur","heron","tahr","fossa","saola","quokka","magpie","oriole","bongo"
};

struct StoredId {
  String name;
  String mac;
  String priv_pem;        // RSA PRIVATE KEY (PEM)
  String pub_pem;         // RSA PUBLIC KEY (PEM)
  String fingerprint_hex; // hex(sha256(trim(pub_pem)))
  String privatekey_hex;
  String session_id;      // aa:bb:cc:dd:ee:ff (first 6 bytes of fingerprint)
};

static String toHex(const uint8_t* buf, size_t len) {
  static const char* H = "0123456789abcdef";
  String out; out.reserve(len*2);
  for (size_t i=0;i<len;i++) { out += H[(buf[i]>>4)&0xF]; out += H[buf[i]&0xF]; }
  return out;
}

static String getBaseMAC() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  String s(buf); s.toLowerCase(); return s;
}

static uint32_t rand32() { uint32_t r; esp_fill_random(&r, sizeof(r)); return r; }

static String sha256hex_bytes(const uint8_t* data, size_t len) {
  uint8_t out[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
  return toHex(out, sizeof(out));
}

static String sha256hex_string_trimmed_newlines(String s) {
  while (s.endsWith("\n")) s.remove(s.length()-1);
  return sha256hex_bytes((const uint8_t*)s.c_str(), s.length());
}

static String genNickname() {
  uint32_t r1 = rand32(), r2 = rand32();
  String seed = String(r1) + ":" + String(r2) + ":" + getBaseMAC();
  // We just want a stable-ish pick across boots; hashing not strictly needed here.
  // Use 4 nibbles from seed hash to index adjective/animal.
  String h = sha256hex_bytes((const uint8_t*)seed.c_str(), seed.length());
  auto nib = [&](int i)->uint8_t {
    char c = h[i]; return (c>='0'&&c<='9')? c-'0' : (c>='a'&&c<='f')? c-'a'+10 : 0;
  };
  uint8_t a = (nib(0)<<4)|nib(1);
  uint8_t b = (nib(2)<<4)|nib(3);
  const size_t A = sizeof(ADJ)/sizeof(ADJ[0]);
  const size_t B = sizeof(ANM)/sizeof(ANM[0]);
  return "Gotchi-" + String(ADJ[a % A]) + "-" + String(ANM[b % B]);
}

// ---- RSA keypair generation and PEM export (mbedTLS) ----

static bool generate_rsa_pem(String& out_priv_pem, String& out_pub_pem, int bits = 2048) {
  mbedtls_pk_context pk;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;

  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  const char* pers = "rsa_gen";
  int rc = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 (const unsigned char*)pers, strlen(pers));
  if (rc != 0) goto cleanup;

  rc = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (rc != 0) goto cleanup;

  rc = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random, &ctr_drbg, bits, 65537);
  if (rc != 0) goto cleanup;

  // Write private key PEM
  {
    // 2048-bit PEM fits well within 1700 bytes; be generous.
    std::unique_ptr<unsigned char[]> buf(new unsigned char[2048]);
    size_t olen = 0;
    rc = mbedtls_pk_write_key_pem(&pk, buf.get(), 2048);
    if (rc != 0) goto cleanup;
    olen = strlen((const char*)buf.get());
    out_priv_pem = String((const char*)buf.get()).substring(0, olen);
  }

  // Write public key PEM (PKIX)
  {
    std::unique_ptr<unsigned char[]> buf(new unsigned char[1200]);
    size_t olen = 0;
    rc = mbedtls_pk_write_pubkey_pem(&pk, buf.get(), 1200);
    if (rc != 0) goto cleanup;
    olen = strlen((const char*)buf.get());
    out_pub_pem = String((const char*)buf.get()).substring(0, olen);
  }

cleanup:
  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  return rc == 0;
}

static String session_from_pub_pem(const String& pub_pem) {
  // pwngrid: Fingerprint = sha256(cleanPEM); session_id = first 6 bytes
  String fhex = sha256hex_string_trimmed_newlines(pub_pem);
  // take first 12 hex chars -> 6 bytes
  if (fhex.length() < 12) return String("");
  char buf[18];
  snprintf(buf, sizeof(buf), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           fhex[0],fhex[1], fhex[2],fhex[3], fhex[4],fhex[5],
           fhex[6],fhex[7], fhex[8],fhex[9], fhex[10],fhex[11]);
  return String(buf);
}

// ---- NVS persistence ----

static bool saveIdentity(const StoredId& id) {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return false;
  p.putString("name", id.name);
  p.putString("mac",  id.mac);
  p.putString("priv_pem", id.priv_pem);
  p.putString("pub_pem",  id.pub_pem);
  p.putString("fpr_hex",  id.fingerprint_hex);
  p.putString("priv_hex", id.privatekey_hex);
  p.putString("sid",      id.session_id);
  p.end();
  return true;
}

static bool loadIdentity(StoredId& out) {
  Preferences p;
  if (!p.begin(NVS_NS, true)) return false;
  String priv = p.getString("priv_pem", "");
  String pub  = p.getString("pub_pem",  "");
  if (priv.isEmpty() || pub.isEmpty()) { p.end(); return false; }
  out.name            = p.getString("name", "");
  out.mac             = p.getString("mac",  "");
  out.priv_pem        = priv;
  out.pub_pem         = pub;
  out.privatekey_hex  = p.getString("priv_hex");
  out.fingerprint_hex = p.getString("fpr_hex", "");
  out.session_id      = p.getString("sid", "");
  p.end();
  return true;
}

// ---- Public API expected by your codebase ----

PwnIdentity ensurePwnIdentity(bool /*recompute_pub_if_missing*/) {
  StoredId sid;
  if (loadIdentity(sid)) {
    // Recompute fingerprint/session if missing or PEM changed
    String phex = sha256hex_string_trimmed_newlines(sid.priv_pem);
    String fhex = sha256hex_string_trimmed_newlines(sid.pub_pem);
    if (sid.fingerprint_hex != fhex || sid.session_id.isEmpty()) {
      sid.fingerprint_hex = fhex;
      sid.session_id = session_from_pub_pem(sid.pub_pem);
      saveIdentity(sid);
    }
  } else {
    // Fresh: make RSA keypair + fields
    sid.name = genNickname();
    sid.mac  = getBaseMAC();

    if (!generate_rsa_pem(sid.priv_pem, sid.pub_pem, 2048)) {
      Serial.println(F("[PWN-ID] RSA generation failed"));
      // leave empty; caller can handle
    }

    sid.privatekey_hex = sha256hex_string_trimmed_newlines(sid.priv_pem);
    sid.fingerprint_hex = sha256hex_string_trimmed_newlines(sid.pub_pem);
    sid.session_id      = session_from_pub_pem(sid.pub_pem);
    saveIdentity(sid);

    Serial.println(F("[PWN-ID] Generated new Grid RSA identity"));
    Serial.printf ("  name        : %s\n", sid.name.c_str());
    Serial.printf ("  mac         : %s\n", sid.mac.c_str());
    Serial.printf ("  private key: %s\n", sid.privatekey_hex.c_str());
    Serial.printf ("  fingerprint : %s\n", sid.fingerprint_hex.c_str());
    Serial.printf ("  session_id  : %s\n", sid.session_id.c_str());
  }

  // Map to your original struct used elsewhere
  PwnIdentity id;
  id.name        = sid.name;
  id.device_mac  = sid.mac;
  // For backward compatibility, put PEMs into these fields:
  id.priv_hex    = sid.priv_pem;        // now PEM text, not hex
  id.pub_hex     = sid.pub_pem;         // PEM
  id.short_id    = sid.session_id;      // MAC-like session id
  return id;
}

// Convenience getters you can call elsewhere (optional)
String getPublicPEM() {
  Preferences p; if (!p.begin(NVS_NS, true)) return "";
  String s = p.getString("pub_pem", ""); p.end(); return s;
}
String getPrivatePEM() {
  Preferences p; if (!p.begin(NVS_NS, true)) return "";
  String s = p.getString("priv_pem", ""); p.end(); return s;
}
String getFingerprintHex() {
  Preferences p; if (!p.begin(NVS_NS, true)) return "";
  String s = p.getString("fpr_hex", ""); p.end(); return s;
}

String getSessionId() {
  Preferences p; if (!p.begin(NVS_NS, true)) return "";
  String s = p.getString("sid", ""); p.end(); return s;
}
