#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *base;    // buffer
  uint32_t bytes;   // per slot
  uint16_t frame_id;// last published frame
  uint16_t in_use;  // 0 = free, 1 = holds frame
} frame_slot_t;

typedef struct {
  frame_slot_t *slot;
  uint16_t nslots;    // number of slots
  uint32_t max_bytes;
  uint8_t *arena;     // memory address
} frame_pool_t;

// basics
int fp_init(frame_pool_t *fp, uint16_t nslots, uint32_t max_bytes);
void fp_free(frame_pool_t *fp);

// producer: aquire a free slot
int fp_aquire(frame_pool_t *fp, uint16_t *slot_out, uint8_t **ptr_out);
// producer: publish bytes and bump generation (available for readers)
int fp_publish(frame_pool_t *fp, uint16_t slot, uint32_t bytes);
// consumer: map an id to a ptr/size if gen matches
int fp_map(frame_pool_t *fp, uint32_t frame_id, uint8_t **ptr_out,
           uint32_t *bytes_out);
