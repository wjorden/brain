#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "test_common.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  rb_t *q = rb_create(8, sizeof(envelope_t));
  assert(q);

  // push 3, pop 3
  envelope_t a = make_in(MOD_IO_TEXT, TOP_INPUT_TEXT, PRI_P2, 1, "a");
  envelope_t b = make_in(MOD_IO_TEXT, TOP_INPUT_TEXT, PRI_P2, 2, "b");
  envelope_t c = make_in(MOD_IO_TEXT, TOP_INPUT_TEXT, PRI_P2, 3, "c");
  assert(rb_try_push(q, &a));
  assert(rb_try_push(q, &b));
  assert(rb_try_push(q, &c));

  envelope_t out;
  assert(rb_try_pop(q, &out) && out.corr == 1);
  assert(rb_try_pop(q, &out) && out.corr == 2);
  assert(rb_try_pop(q, &out) && out.corr == 3);
  assert(!rb_try_pop(q, &out));

  (void)a;
  (void)b;
  (void)c;
  (void)out;
  rb_destroy(q);
  printf("[OK] ring sanity\n");
  return 0;
}
