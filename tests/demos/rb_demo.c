#include "interfaces/bus/broker.h"
#include "interfaces/bus/event.h"
#include "interfaces/bus/rb.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

typedef struct {
  rb_t *out; // producer writes this ring
  int count;
} producer_arg_t;

static _Atomic int done_producing = 0;
static _Atomic int broker_closed = 0;

void *broker_thread_fn(void *arg) {
  broker_t *br = (broker_t *)arg;
  for (;;) {
    broker_pump(br);
    // exit condition for demo
    if (atomic_load(&done_producing) && rb_empty(br->in)) {
      event_t eos = {0};
      eos.type = EVENT_END;
      eos.ts_ms = now_ms();
      for (size_t i = 0; i < br->n_outs; ++i) {
        rb_t *outq = br->outs[i];
        while (!rb_try_push(outq, &eos)) {
          usleep(1000);
        }
      }
      atomic_store(&broker_closed, 1);
      break;
    }
    usleep(1000);
  }
  return NULL;
}

void *producer_thread(void *arg) {
  producer_arg_t *a = (producer_arg_t *)arg;
  for (int i = 0; i < a->count; i++) {
    event_t ev = {0};
    ev.type = EVENT_INPUT_TEXT;
    ev.ts_ms = now_ms();
    const char *s = (i % 3 == 0) ? "status" : (i % 3 == 1 ? "stop" : "help");
    ev.len = (uint32_t)snprintf(ev.data, EVENT_DATA_MAX, "%s", s);
    while (!rb_try_push(a->out, &ev)) {
      sched_yield();
    }
    usleep(1000);
  }
  atomic_store(&done_producing, 1);
  return NULL;
}

typedef struct {
  rb_t *in;
  const char *name;
  int processed;
} consumer_arg_t;

void *consumer_thread(void *arg) {
  consumer_arg_t *c = (consumer_arg_t *)arg;
  event_t ev;
  for (;;) {
    if (rb_try_pop(c->in, &ev)) {
      c->processed++;
      switch (ev.type) {
      case EVENT_END:
        // TODO: re-inject for other readers of same queue
        break;
      case EVENT_INPUT_TEXT:
        printf("[%s] got INPUT_TEXT: '%.*s' @ %llu ms\n", c->name, (int)ev.len,
               ev.data, (unsigned long long)ev.ts_ms);
        break;
      }
    } else {
      if (atomic_load(&broker_closed) && rb_empty(c->in))
        break;
      // idle
      usleep(2000);
    }
  }
  return NULL;
}

int main(void) {
  // main input ring
  rb_t *main = rb_create(256, sizeof(event_t));

  // two subscriber rings
  rb_t *sub1 = rb_create(128, sizeof(event_t));
  rb_t *sub2 = rb_create(128, sizeof(event_t));
  rb_t *outs[2] = {sub1, sub2};

  broker_t *b = broker_create(main, outs, 2);

  // threads
  pthread_t pt, bt, c1, c2;

  // producer -> main (100 events)
  producer_arg_t pa = {.out = main, .count = 100};
  pthread_create(&pt, NULL, producer_thread, &pa);

  // broker fan
  pthread_create(&bt, NULL, broker_thread_fn, b);

  // two consumers
  consumer_arg_t ca1 = {.in = sub1, .name = "brainstem"};
  consumer_arg_t ca2 = {.in = sub2, .name = "hippocampus"};
  pthread_create(&c1, NULL, consumer_thread, &ca1);
  pthread_create(&c2, NULL, consumer_thread, &ca2);

  // join producer
  pthread_join(pt, NULL);
  pthread_join(c1, NULL);
  pthread_join(c2, NULL);
  pthread_join(bt, NULL);

  size_t fwd = b->forwarded;
  size_t drops = b->drops;
  printf("[BROKER] forwarded=%zu drops=%zu\n", fwd, drops);

  // cleanup -> reverse of creation
  broker_destroy(b);
  rb_destroy(sub2);
  rb_destroy(sub1);
  rb_destroy(main);
  return 0;
}
