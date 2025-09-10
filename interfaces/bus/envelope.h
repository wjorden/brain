#pragma once
#include <stddef.h>
#include <stdint.h>

#define ENVELOPE_MAGIC 0x51524F52u /* QROR */
#define ENVELOPE_TAIL 0x45414F52u  /* EAOR */

typedef enum { PRI_P0 = 0, PRI_P1 = 1, PRI_P2 = 2, PRI_P3 = 3 } pri_t;

typedef enum {
  MOD_UNKNOWN = 0,
  MOD_IO_TEXT = 1,

  // big brain
  MOD_CORTEX = 2,
  MOD_VISUAL_CORTEX = 3,
  MOD_BRAINSTEM = 4,

  MOD_BASAL_GANGLIA = 5,
  MOD_AUTONOMIC = 6,
  MOD_AMYGDALA = 7,
  MOD_THALAMUS_VISION = 8,
  MOD_HYPOCAMPUS_MEDIA = 9,

  // out -> in
  MOD_PERIPHERY_VOICE = 10,
  MOD_PERIPHERY_CAMERA = 11,
} module_t;

typedef enum {
  TOP_NONE = 0,

  // southbound
  TOP_INPUT_TEXT = 10,
  TOP_OUTPUT_TEXT = 11,
  TOP_MOTOR_MORSE = 12,
  TOP_VITALS_SNAPSHOT = 13,

  // northbound
  TOP_INTENT_STATUS = 20,
  TOP_INTENT_STOP = 21,
  TOP_INTENT_REPEAT = 22,
  TOP_INTENT_HELP = 23,
  TOP_DECISION_SAY = 24,

  // control
  TOP_CONTROL_AS = 30, // standby
  TOP_CONTROL_BK = 31, // handover
  TOP_CONTROL_AR = 32, // end of message
  TOP_CONTROL_SK = 33, // abort/hard stop

  // visual
  TOP_FRAME_AVAILABLE = 40,
  TOP_VISION_MOTION = 41,
  TOP_VISION_STATS = 42,
  TOP_VISION_THUMB = 43,

  // health stuff
  TOP_HEALTH_PING = 100, // large number so it's out of the way
  TOP_HEALTH_VISION = 101,
} topic_t;

// envelope should be <= 256 for ring buffer
typedef struct {
  uint32_t magic;    // must be ENEVLOPE_MAGIC /*0*/
  uint64_t id;       // monotonic sequence (from producer) /*8*/
  uint64_t ts_ms;    // monotonic ms /*16*/
  uint64_t corr;     // correlation id /*24*/
  uint32_t src;      // module_t /*32*/
  uint32_t dst;      // module_t or 0 for broadcast /*36*/
  uint16_t topic;    // topic_t /*40*/
  uint8_t pri;       // pri_t /*42*/
  uint8_t schema_v;  // payload schema version /*43*/
  uint32_t flags;    // bit flags /*44*/
  uint32_t len;      // bytes used in data[] /*48*/
  uint8_t data[200]; // payload UTF-8, memcopy /*52..251*/
  uint32_t tail;     // must be ENVELOPE_TAIL /*252*/
} envelope_t;

// layout checks, compile time
_Static_assert(sizeof(envelope_t) == 256, "envelope_t must be 256 bytes");
_Static_assert(offsetof(envelope_t, magic) == 0, "magic at 0");
_Static_assert(offsetof(envelope_t, id) == 8, "id at 8");
_Static_assert(offsetof(envelope_t, ts_ms) == 16, "ts_ms at 16");
_Static_assert(offsetof(envelope_t, corr) == 24, "corr at 24");
_Static_assert(offsetof(envelope_t, src) == 32, "src at 32");
_Static_assert(offsetof(envelope_t, dst) == 36, "dst at 36");
_Static_assert(offsetof(envelope_t, topic) == 40, "topic at 40");
_Static_assert(offsetof(envelope_t, pri) == 42, "pri at 42");
_Static_assert(offsetof(envelope_t, schema_v) == 43, "scheme_v at 43");
_Static_assert(offsetof(envelope_t, flags) == 44, "flags at 44");
_Static_assert(offsetof(envelope_t, len) == 48, "len at 48");
_Static_assert(offsetof(envelope_t, data) == 52, "data at 52");
_Static_assert(offsetof(envelope_t, tail) == 252, "tail at 252");

_Static_assert(sizeof(envelope_t) >= 248 && sizeof(envelope_t) <= 256,
               "envelope size drifted; update rings/copies accordingly");
