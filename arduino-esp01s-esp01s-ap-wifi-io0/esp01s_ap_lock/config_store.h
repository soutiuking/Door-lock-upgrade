/*
 * ESP8266 lock configuration storage.
 *
 * Four Flash sectors are used as a rotating record log. The final five
 * sectors are reserved for ESP8266 SDK/RF data. Every record has a sequence
 * number and CRC16 and is read back after writing before success is reported.
 *
 * This layout must be revised if LittleFS or SPIFFS is enabled later.
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>

#define LOCK_CONFIG_VERSION 1
#define LOCK_MAX_CODES      10

struct __attribute__((packed)) LockConfig {
  uint16_t version;
  char     adminKey[20];
  uint32_t codes[LOCK_MAX_CODES];
  uint16_t winStart;
  uint16_t winEnd;
  uint8_t  debugMode;
  char     uplinkSsid[33];
  char     uplinkPass[65];
  uint16_t lockAngle;
  uint16_t unlockAngle;
  uint16_t unlockHoldMs;
  uint8_t  reserved[73];
};
static_assert(sizeof(LockConfig) <= 244, "LockConfig too big for one record");

#define STORE_MAGIC     0x4C4Bu
#define STORE_REC_SIZE  256
#define STORE_SECTORS          4
#define STORE_SYSTEM_SECTORS   5   /* Reserved SDK/RF sectors at end of Flash */

struct __attribute__((packed, aligned(4))) StoreRec {
  uint32_t seq;
  uint16_t magic;
  uint16_t len;
  uint16_t crc;
  uint16_t rsvd;
  uint8_t  data[244];
};
static_assert(sizeof(StoreRec) == STORE_REC_SIZE, "StoreRec must be 256 bytes");

class ParamStore {
public:
  bool begin() {
    ok_ = false;
    legacyLoaded_ = false;
    lastSeq_ = 0;
    uint32_t flashSize = ESP.getFlashChipSize();
    if (flashSize < (STORE_SECTORS + STORE_SYSTEM_SECTORS + 8) * 4096UL) return false;

    // New area: before the reserved SDK/RF sectors.
    base_ = flashSize - (uint32_t)(STORE_SECTORS + STORE_SYSTEM_SECTORS) * 4096UL;
    // Previous firmware wrote at the last four sectors. Read it once and
    // migrate a valid record on the first boot after this firmware update.
    legacyBase_ = flashSize - (uint32_t)STORE_SECTORS * 4096UL;
    slots_ = ((uint32_t)STORE_SECTORS * 4096UL) / STORE_REC_SIZE;
    if (base_ < ESP.getSketchSize() + 0x1000UL) return false;

    ok_ = true;
    scanRegion(base_);
    scanRegion(legacyBase_);
    return true;
  }

  bool ok() const { return ok_; }
  uint32_t seq() const { return lastSeq_; }
  uint32_t writes() const { return writes_; }
  uint32_t erases() const { return erases_; }
  uint32_t slots() const { return slots_; }
  bool loadedFromLegacy() const { return legacyLoaded_; }

  bool load(void* buf, size_t len) {
    if (!ok_ || len > 244) return false;
    StoreRec r, best;
    bool found = false;
    uint32_t bestBase = 0;
    for (uint8_t region = 0; region < 2; region++) {
      uint32_t regionBase = region == 0 ? base_ : legacyBase_;
      for (uint32_t s = 0; s < slots_; s++) {
        if (!readRecAt(regionBase, s, r)) continue;
        if (!validRec(r) || r.len != len) continue;
        if (!found || r.seq > best.seq) {
          best = r;
          bestBase = regionBase;
          found = true;
        }
      }
    }
    if (!found) return false;
    memcpy(buf, best.data, len);
    lastSeq_ = best.seq;
    legacyLoaded_ = (bestBase == legacyBase_);
    return true;
  }

  bool save(const void* buf, size_t len) {
    if (!ok_ || len > 244) return false;
    uint32_t seq  = lastSeq_ + 1;
    uint32_t slot = seq % slots_;
    uint32_t addr = base_ + slot * STORE_REC_SIZE;

    StoreRec r;
    memset(&r, 0xFF, sizeof(r));
    r.seq = seq;
    r.magic = STORE_MAGIC;
    r.len = (uint16_t)len;
    r.crc = crc16((const uint8_t*)buf, (uint16_t)len);
    memcpy(r.data, buf, len);

    StoreRec probe;
    bool erased = true;
    if (readRec(slot, probe)) erased = (probe.magic == 0xFFFF);
    if (!erased) {
      if (!ESP.flashEraseSector(addr / 4096UL)) return false;
      erases_++;
    }
    if (!ESP.flashWrite(addr, (uint32_t*)&r, sizeof(r))) return false;

    // Read back and verify before reporting a successful save.
    StoreRec verify;
    if (!readRec(slot, verify)) return false;
    if (verify.seq != r.seq || verify.magic != STORE_MAGIC || verify.len != r.len) return false;
    if (verify.crc != r.crc || crc16(verify.data, verify.len) != verify.crc) return false;
    if (memcmp(verify.data, r.data, len) != 0) return false;

    writes_++;
    lastSeq_ = seq;
    return true;
  }

private:
  bool readRecAt(uint32_t regionBase, uint32_t slot, StoreRec& r) {
    if (regionBase == 0 || slots_ == 0 || slot >= slots_) return false;
    return ESP.flashRead(regionBase + slot * STORE_REC_SIZE, (uint32_t*)&r, sizeof(r));
  }

  bool readRec(uint32_t slot, StoreRec& r) {
    return readRecAt(base_, slot, r);
  }

  static bool validRec(const StoreRec& r) {
    if (r.magic != STORE_MAGIC || r.len > sizeof(r.data)) return false;
    return crc16(r.data, r.len) == r.crc;
  }

  void scanRegion(uint32_t regionBase) {
    StoreRec r;
    for (uint32_t s = 0; s < slots_; s++) {
      if (!readRecAt(regionBase, s, r)) continue;
      if (!validRec(r)) continue;
      if (r.seq > lastSeq_) lastSeq_ = r.seq;
    }
  }

  static uint16_t crc16(const uint8_t* p, uint16_t n) {
    uint16_t crc = 0xFFFF;
    while (n--) {
      crc ^= (uint16_t)(*p++) << 8;
      for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
  }

  bool ok_ = false;
  uint32_t base_ = 0;
  uint32_t legacyBase_ = 0;
  uint32_t slots_ = 0;
  uint32_t lastSeq_ = 0;
  bool legacyLoaded_ = false;
  uint32_t writes_ = 0;
  uint32_t erases_ = 0;
};

#endif
