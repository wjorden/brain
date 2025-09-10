#include "frame_pool.h"
#include "interfaces/io/vision_payloads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fp_init(frame_pool_t *fp, uint16_t nslots, uint32_t max_bytes) {
    // no point in moving forward if no mem is required
    if (nslots == 0 || max_bytes == 0)
    return -1;

    // make sure we do not already have this one
  if (fp->arena || fp->slot)
    fp_free(fp);

  // clean data arena
  memset(fp, 0, sizeof *fp);
  fp->nslots = nslots;
  fp->max_bytes = max_bytes;
  // first the arena
  fp->arena = (uint8_t *)malloc((size_t)nslots * max_bytes);
  if (!fp->arena) {
    // leave everything as zero and exit
    fp->nslots = fp->max_bytes = 0;
    return -2;
  }

  // then the slot
  fp->slot = (frame_slot_t *)calloc(nslots, sizeof(frame_slot_t));
  if (!fp->slot) {
    free(fp->arena);
    fp->arena = NULL;
    fp->nslots = fp->max_bytes = 0;
    return -3;
  }

  for (uint16_t i = 0; i < nslots; i++) {
    fp->slot[i].base = fp->arena + ((size_t)i * max_bytes);
    fp->slot[i].frame_id = 1;
    fp->slot[i].in_use = 0;
    fp->slot[i].bytes = 0;
  }
  return 0;
}

void fp_free(frame_pool_t *fp) {
  if (!fp)
    return;
  if (fp->slot) {
    free(fp->slot);
    fp->slot = NULL;
  }
  if (fp->arena) {
    free(fp->arena);
    fp->arena = NULL;
  }
  fp->nslots = 0;
  fp->max_bytes = 0;
}

// round robin
static uint16_t rr_next = 0;
int fp_aquire(frame_pool_t *fp, uint16_t *slot_out, uint8_t **ptr_out) {
  for (uint16_t i = 0; i < fp->nslots; i++) {
    uint16_t s = (uint16_t)(rr_next + i) % fp->nslots;
    if (!fp->slot[i].in_use) {
      fp->slot[i].in_use = 1;
      *slot_out = s;
      *ptr_out = fp->slot[i].base;
      rr_next = (uint16_t)((s + 1) % fp->nslots);
      return 0;
    }
  }
  return -1;
}

int fp_publish(frame_pool_t *fp, uint16_t slot, uint32_t bytes)
{
    if (slot >= fp->nslots || bytes > fp->max_bytes) {
        printf("[FP] publish slot(%u) >= fp->nslots(%u) or bytes(%u) > "
               "fp->max_bytes(%u)\n",
               slot,
               fp->nslots,
               bytes,
               fp->max_bytes);
        return -1;
    }

    fp->slot[slot].bytes = bytes;
    fp->slot[slot].frame_id++;
    fp->slot[slot].in_use = 0;
    printf("[FP] publish slot=%u bytes=%u gen=%u in_use=%u\n",
           slot,
           fp->slot->bytes,
           frame_gen(fp->slot->frame_id),
           fp->slot->in_use);
    return 0;
}

int fp_map(frame_pool_t *fp, uint32_t frame_id, uint8_t **ptr_out,
           uint32_t *bytes_out) {
  uint16_t slot = frame_slot(frame_id);
  uint16_t gen = frame_gen(frame_id);
  if (slot >= fp->nslots) {
    printf("[FP] map slot=%u >= nslots=%u\n", slot, fp->nslots);
    return -1;
  }
  if (fp->slot[slot].frame_id != gen) {
    printf("[FP] map frame_id=%u != gen=%u\n", frame_id, gen);
    return -2;
  }
  *ptr_out = fp->slot[slot].base;
  *bytes_out = fp->slot[slot].bytes;
  printf("[FP] map id=%u slot=%u gen=%u\n", (unsigned) frame_id, (unsigned) slot, (unsigned) gen);
  return 0;
}
