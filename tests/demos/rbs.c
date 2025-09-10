#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  rb_t *q = rb_create(8, sizeof(envelope_t));
  if (!q) {
    puts("ring alloc failed.");
    return 2;
  }

  envelope_t a, b;
  memset(&a, 0, sizeof a);
  a.magic = ENVELOPE_MAGIC;
  a.tail = ENVELOPE_TAIL;
  a.id = 42;
  a.topic = TOP_DECISION_SAY;
  a.pri = PRI_P2;
  a.corr = 999;
  a.len = (unsigned)snprintf((char *)a.data, sizeof a.data, "hello");

  if (!rb_try_push(q, &a)) {
    puts("push fail");
    return 3;
  }
  if (!rb_try_pop(q, &b)) {
    puts("pop fail");
    return 4;
  }

  printf("magic=0x%08x tail=0x%08x id=%llu corr=%llu topic=%u pri=%u "
         "data='%.*s'\n",
         b.magic, b.tail, (unsigned long long)b.id, (unsigned long long)b.corr,
         b.topic, b.pri, (int)b.len, b.data);

  return (b.magic == ENVELOPE_MAGIC && b.tail == ENVELOPE_TAIL && b.id == 42 &&
          b.corr == 999 && b.topic == TOP_DECISION_SAY && b.pri == PRI_P2)
             ? 0
             : 1;
}
