#pragma once
#include <stdint.h>

enum {
  EVENT_NONE = 0,
  EVENT_INPUT_TEXT,
  EVENT_OUTPUT_TEXT,
  EVENT_OUTPUT_CONTROL, // AS/BK/AR/SK, etc
  EVENT_VITALS,
  EVENT_END, // end-of-stream (sentinel)
  // additional as needed
};

#define EVENT_DATA_MAX 240 // keep struct <= 256 bytes

typedef struct event_t {
  uint16_t type;
  uint16_t flags;            // priority hint, truncated flag
  uint32_t len;              // bytes in data
  uint64_t ts_ms;            // monotonic ms
  char data[EVENT_DATA_MAX]; // payload (UTF-8, small structs via memcpy)
} event_t;
// NOTE: sizeof(event_t) == 256 on most platforms
