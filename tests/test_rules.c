#include "brain/brainstem/brainstem.h"
#include "interfaces/bus/envelope.h"
#include "test_common.h"
#include <stdio.h>

extern const char *BRAINSTEM_BUILD_ID;
static void test_defer_once(void) {
  fprintf(stderr, "[SIG] defer_once: %s\n", BRAINSTEM_BUILD_ID);
  brainstem_t bs;
  rb_t *in, *out;
  bs_setup(&bs, &in, &out, 256, 256);
  g_now_ms = 1000;

  // 1) brain says something (open utterance)
  envelope_t say =
      make_in(MOD_PERIPHERY_VOICE, TOP_DECISION_SAY, PRI_P2, 1001, NULL);

  must_push(in, &say);
  size_t em = brainstem_tick(&bs, now_ms());
  assert(em == 0);

  // 2) user P2 input -> expect CONTROL_AS (30)
  envelope_t p2 = make_in(MOD_IO_TEXT, TOP_INPUT_TEXT, PRI_P2, 1002, "status");

  must_push(in, &p2);
  em = brainstem_tick(&bs, now_ms());
  assert(em >= 1);

  envelope_t outs[8];
  size_t n = pop_all(out, outs, 8);
  // expect at least one control for 1002
  bool saw_as = false;
  for (size_t i = 0; i < n; i++)
    if (outs[i].topic == TOP_CONTROL_AS && outs[i].corr == 1002)
      saw_as = true;
  assert(saw_as && "expected stand-by defer");

  // 3) no new inputs
  for (int k = 0; k < 3; k++) {
    g_now_ms += 5;
    em = brainstem_tick(&bs, now_ms());
  }
  n = pop_all(out, outs, 8);
  assert(n == 0 && "no duplicate stand-by allowed");

  (void)em;
  (void)saw_as;
  bs_teardown(&bs, in, out);
}

static void test_interrupt_ack_and_close(void) {
  fprintf(stderr, "[SIG] ack and close: %s\n", BRAINSTEM_BUILD_ID);
  brainstem_t bs;
  rb_t *in, *out;
  bs_setup(&bs, &in, &out, 256, 256);
  g_now_ms = 2000;

  // 1) say
  envelope_t e =
      make_in(MOD_PERIPHERY_VOICE, TOP_DECISION_SAY, PRI_P2, 2001, NULL);

  must_push(in, &e);
  size_t em = brainstem_tick(&bs, now_ms());
  assert(em >= 3);

  envelope_t outs[8];
  size_t n = pop_all(out, outs, 8);
  bool saw_sk = false, saw_ak = false, saw_ar = false;
  for (size_t i = 0; i < n; i++) {
    if (outs[i].topic == TOP_CONTROL_SK && outs[i].corr == 2001)
      saw_sk = true;
    if (outs[i].topic == TOP_OUTPUT_TEXT && outs[i].corr == 2002)
      saw_ak = true;
    if (outs[i].topic == TOP_CONTROL_AR && outs[i].corr == 2003)
      saw_ar = true;
  }
  assert(saw_sk && saw_ak && saw_ar);

  // no dupes
  for (int k = 0; k < 3; k++) {
    g_now_ms += 5;
    brainstem_tick(&bs, now_ms());
  }
  n = pop_all(out, outs, 8);
  assert(n == 0);
  (void)saw_sk, (void)saw_ak;
  (void)saw_ar;
  (void)em;
  bs_teardown(&bs, in, out);
}

static void test_window_expirary(void) {
  fprintf(stderr, "[SIG] window exp: %s\n", BRAINSTEM_BUILD_ID);
  brainstem_t bs;
  rb_t *in, *out;
  bs_setup(&bs, &in, &out, 256, 256);
  bs.window_ms = 10;
  g_now_ms = 3000;

  // say then wait
  envelope_t e =
      make_in(MOD_PERIPHERY_VOICE, TOP_DECISION_SAY, PRI_P2, 3001, NULL);

  must_push(in, &e);
  brainstem_tick(&bs, now_ms());
  g_now_ms += 20;

  // now p2 input should NOT produce AS
  e = make_in(MOD_IO_TEXT, TOP_INPUT_TEXT, PRI_P2, 3002, "status");

  must_push(in, &e);
  size_t em = brainstem_tick(&bs, now_ms());
  envelope_t outs[8];
  size_t n = pop_all(out, outs, 8);

  bool saw_as = false;
  for (size_t i = 0; i < n; i++)
    if (outs[i].topic == TOP_CONTROL_AS && outs[i].corr == 3002)
      saw_as = true;
  assert(!saw_as && "defer must not trigger after window expiry");

  (void)em;
  (void)saw_as;
  bs_teardown(&bs, in, out);
}

int main(void) {
  test_defer_once();
  test_interrupt_ack_and_close();
  test_window_expirary();
  printf("[OK] all tests passed\n");
  return 0;
}
