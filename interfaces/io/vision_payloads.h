#pragma once

#include <stdint.h>

typedef enum {
  SCH_FRAME = 1,
  SCH_MOTION = 2,
  SCH_STATS = 3,
  SCH_THUMB = 4,
  SCH_HEALTH = 5,
} schema_t;

#pragma pack(push, 1)

typedef struct {
  uint32_t frame_id; // gen < 16 || slot
  uint16_t w, h;
  uint16_t stride;
  uint8_t fmt; // 0 = GRAY8, 1 = YUYV
  uint8_t reserved;
  uint64_t ts_ms;
} pl_frame; // 16B

typedef struct {
  uint32_t frame_id;
  uint16_t level; // 0..100 (percent * 10)
  uint16_t
      hot_blocks; // number of 8x8 blocks over threshold (brightness/intensity)
  uint16_t bbox_x, bbox_y, bbox_w, bbox_h; // 0 if none
} pl_motion;                               // 16B

typedef struct {
  uint32_t frame_id;
  uint16_t avg_luma; // scaled (x4) average 0..100
  uint16_t var_luma; // scaled variance
  uint16_t dark_pct; // 0..1000
  uint16_t sat_pct;  // 0..1000
} pl_stats;          // 12B

typedef struct {
  uint32_t frame_id;
  uint16_t tw, th; // thumbnail dims (96x64)
  uint32_t crc32;
} pl_thumb; // 12B

typedef struct {
  uint16_t kind;  // 1=pool_overrun 2=stale_frame 3 = late_proc
  uint16_t value; // counter/code
  uint32_t aux;   // optional
} pl_health;      // 8B

#pragma pack(pop)

_Static_assert(sizeof(pl_frame) <= 64, "pl_frame too big");
_Static_assert(sizeof(pl_motion) <= 64, "pl_motion too big");
_Static_assert(sizeof(pl_stats) <= 64, "pl_stats too big");
_Static_assert(sizeof(pl_thumb) <= 64, "pl_thumb too big");
_Static_assert(sizeof(pl_health) <= 64, "pl_health too big");

#define SLOT_BITS 12u
#define SLOT_MASK ((1u << SLOT_BITS) - 1u)
// helpers
static inline uint32_t frame_slot(uint32_t id) {
  return id & SLOT_MASK;
}
static inline uint32_t frame_gen(uint32_t id) {
  return id >> SLOT_BITS;
}
static inline uint32_t frame_make(uint32_t slot, uint32_t gen) {
  return (gen << SLOT_BITS | (slot & SLOT_MASK));
}
