/* test_cuelist_run.c - `lj4evr_tests test_cuelist_run`: the UI-thread cue list runner (app/lj_cuelist_run.h, design 2.3
 * rule 4, 10 W3 acceptance 1-2) at 10 ms steps against a fake host and synthetic PC time: the 20-line list latches its
 * cues at the right elapsed ms, manual lines ignore their waits and wait for Go, an MP Play line (load_error) is skipped
 * and the rest runs, Goto Line# loops, Goto beyond the last line ends the list, Set Cue Trig posts one TRIG b = 0xFF per
 * slot, Flash Sequence pins and releases, Hotkey Command runs its function, Execute refused without AllowExecute, an
 * MTC wait with no time code waits until Go, PC clock and date-change waits, Back / Jump / marks / Run to line, tracking
 * on latches the earlier cues and tracking off skips them, Link Cuelist, Stop and Clear, Log time code. Then the two
 * LOW engine rows the cue list exercises: LORD_ENGINE-7 (GO / STOP / TRIG on a static runner index is a no-op) and
 * LORD_ENGINE-9 (the Sequence window's scene span after an unfreeze never wraps). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_cuelist_run.h"
#include "engine/lj_engine.h"
#include "model/lj_cuelist.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- the fake host: records every effect with the synthetic time it happened at ---- */
enum { T_EV_MAX = 256 };
typedef struct { char kind; uint32_t a, b; uint64_t t; } t_ev;   /* c cue, t trig(slot), f flash(seq, on), g bg, h fn, x exec, k link, z clear */
static struct { t_ev ev[T_EV_MAX]; int n; uint64_t now; uint32_t clock_base_ms, day; int mtc_ok; uint32_t mtc_ms; const lj_cuelist *linked; } T;

static void t_rec(char kind, uint32_t a, uint32_t b) { if (T.n < T_EV_MAX) { T.ev[T.n].kind = kind; T.ev[T.n].a = a; T.ev[T.n].b = b; T.ev[T.n].t = T.now; } T.n++; }
static int  t_load_cue(void *ud, uint32_t n) { (void)ud; t_rec('c', n, 0); return n == 404 ? -1 : 0; }
static int  t_post(void *ud, const lj_cmd *c) { (void)ud; if (c->kind == LJ_CMD_TRIG) t_rec('t', c->a, c->b); return 0; }
static int  t_load_bg(void *ud, uint32_t n) { (void)ud; t_rec('g', n, 0); return 0; }
static int  t_flash(void *ud, uint32_t seq, int on) { (void)ud; t_rec('f', seq, (uint32_t)on); return 0; }
static int  t_link(void *ud, uint32_t n, const lj_cuelist **out) { (void)ud; t_rec('k', n, 0); *out = T.linked; return T.linked ? 0 : -1; }
static void t_fn(void *ud, int fn);
static int  t_exec(void *ud, const char *path, const char *args) { (void)ud; (void)path; (void)args; t_rec('x', 0, 0); return -1; }   /* AllowExecute=0 */
static void t_clear(void *ud) { (void)ud; t_rec('z', 0, 0); }
static int  t_timecode(void *ud, int src, uint32_t *ms) { (void)ud; if (src == LJ_TC_MIDI && T.mtc_ok) { *ms = T.mtc_ms; return 0; } return -1; }
static int  t_clock(void *ud, uint32_t *ms_of_day, uint32_t *day)
{
    uint64_t ms = (uint64_t)T.clock_base_ms + T.now / 1000u;
    (void)ud; *ms_of_day = (uint32_t)(ms % 86400000u); *day = T.day + (uint32_t)(ms / 86400000u);
    return 0;
}
static int  t_media(void *ud, const lj_cl_line *l, uint64_t now) { (void)ud; (void)now; t_rec('m', l->cmd, 0); return 0; }   /* W5 media: MP lines execute (W5-MED-3) */
static int  t_cue_transparent(void *ud, uint32_t n) { (void)ud; return n >= 100; }   /* ME-171 (g): cues 100.. are transparent */
static const lj_cl_host H = { NULL, t_load_cue, t_post, t_load_bg, t_flash, t_link, t_fn, t_exec, t_clear, t_timecode, t_clock, t_media, t_cue_transparent };
static lj_cl_run R;
static struct { int op, depth, max_depth, rc; } RE;   /* ME-108: a Hotkey Command that drives the runner it runs in */
static void t_fn(void *ud, int fn)
{
    (void)ud; t_rec('h', (uint32_t)fn, 0);
    if (!RE.op) return;
    if (++RE.depth > RE.max_depth) RE.max_depth = RE.depth;
    if (RE.depth < 50) RE.rc = lj_cl_run_op(&R, R.cl, RE.op, 0, T.now, &H);
    RE.depth--;
}

static void t_reset(uint32_t clock_base_ms) { memset(&T, 0, sizeof T); T.clock_base_ms = clock_base_ms; lj_cl_run_init(&R); }
static int t_op(const lj_cuelist *cl, int op, uint32_t n) { return lj_cl_run_op(&R, cl, op, n, T.now, &H); }
/* the app's idle loop: one step per 10 ms wake up to `ms`, the time read once per pass */
static void t_run_to(uint64_t ms) { while (T.now + 10000u <= ms * 1000u) { T.now += 10000u; lj_cl_run_step(&R, T.now, &H); } }
static int t_find(char kind, uint32_t a, int from) { int i; for (i = from; i < T.n && i < T_EV_MAX; i++) if (T.ev[i].kind == kind && T.ev[i].a == a) return i; return -1; }
static long t_at(char kind, uint32_t a) { int i = t_find(kind, a, 0); return i < 0 ? -1 : (long)(T.ev[i].t / 1000u); }   /* ms of the first such event */
static int t_count(char kind) { int i, c = 0; for (i = 0; i < T.n && i < T_EV_MAX; i++) c += T.ev[i].kind == kind; return c; }

static lj_cl_line t_line(int manual, int cmd, uint32_t param, uint32_t ref, uint32_t cue, uint16_t trig)
{
    lj_cl_line l;
    memset(&l, 0, sizeof l);
    l.manual = (uint8_t)manual; l.cmd = (uint8_t)cmd; l.param_ms = param; l.ref = ref; l.cue = cue; l.trig_mask = trig;
    l.load_error = (uint8_t)lj_cl_cmd_inert(cmd);
    return l;
}

/* The 20-line list (the same lines as tests/engine/replay/cuelist_run.lj4r). A line is one command (a Wait is the
 * line's condition, any other command its action) plus a cue latch or slot trigs:
 *  1 auto  -                         cue 1      at Top (0 ms)
 *  2 auto  Wait Elapsed 1.00 s       cue 2      1000 ms
 *  3 MAN   Wait Elapsed 5.00 s       cue 3      the wait is ignored: Go at 2000 ms
 *  4 auto  Wait Elapsed 0.50 s       trig 1+2   2500 ms, one TRIG b = 0xFF per slot
 *  5 auto  MP Play                   cue 4      2500 ms: W5 media, the MP line executes (W5-MED-3; was a load error in v1.0.0)
 *  6 auto  Hotkey Command 7          -          2500 ms
 *  7 auto  Wait Elapsed 0.50 s       -          3000 ms
 *  8 auto  Flash Seq 5, 0.50 s       -          3000 ms, released 3500 ms
 *  9 auto  Wait Elapsed 1.00 s       cue 4      4000 ms
 * 10 auto  Execute File fog.exe      -          refused (AllowExecute=0), the list goes on
 * 11 auto  Load BGCue 1              -          4000 ms
 * 12 MAN   -                         cue 1      Go at 6000 ms
 * 13 auto  Wait MTC 00:00:10:00      cue 2      no time code: waits until Go (8000 ms)
 * 14 auto  Wait Elapsed 1.00 s       cue 3      9000 ms
 * 15 auto  Wait PC-Clock 00:00:10    cue 4      the clock (midnight + t) reaches 10 s at 10000 ms
 * 16 auto  Wait Elapsed 1.00 s       cue 1      11000 ms
 * 17 auto  Goto Line# 19             -          11000 ms
 * 18 auto  -                         cue 4      jumped over
 * 19 auto  Goto Line# 2              -          11000 ms: line 2 waits 1 s (cue 2 at 12000 ms), line 3 waits for Go
 * 20 auto  Goto Line# 99             -          beyond the last line: the list ends (Run from line 20, mark 2) */
static void t_list20(lj_cuelist *cl)
{
    lj_cl_line rows[20];
    int i;
    rows[0]  = t_line(0, LJ_CL_NONE, 0, 0, 1, 0);
    rows[1]  = t_line(0, LJ_CL_WAIT_ELAPSED, 1000, 0, 2, 0);
    rows[2]  = t_line(1, LJ_CL_WAIT_ELAPSED, 5000, 0, 3, 0);
    rows[3]  = t_line(0, LJ_CL_WAIT_ELAPSED, 500, 0, 0, 0x003);
    rows[4]  = t_line(0, LJ_CL_MP_PLAY, 0, 0, 4, 0);
    rows[5]  = t_line(0, LJ_CL_HOTKEY_COMMAND, 7, 0, 0, 0);
    rows[6]  = t_line(0, LJ_CL_WAIT_ELAPSED, 500, 0, 0, 0);
    rows[7]  = t_line(0, LJ_CL_FLASH_SEQUENCE, 500, 5, 0, 0);
    rows[8]  = t_line(0, LJ_CL_WAIT_ELAPSED, 1000, 0, 4, 0);
    rows[9]  = t_line(0, LJ_CL_EXECUTE_FILE, 0, 0, 0, 0); snprintf(rows[9].param, sizeof rows[9].param, "fog.exe");
    rows[10] = t_line(0, LJ_CL_LOAD_BACKGROUND_CUE, 0, 1, 0, 0);
    rows[11] = t_line(1, LJ_CL_NONE, 0, 0, 1, 0);
    rows[12] = t_line(0, LJ_CL_WAIT_MTC, lj_cl_hmsf_ms(0, 0, 10, 0, 25), 0, 2, 0); snprintf(rows[12].param, sizeof rows[12].param, "25");
    rows[13] = t_line(0, LJ_CL_WAIT_ELAPSED, 1000, 0, 3, 0);
    rows[14] = t_line(0, LJ_CL_WAIT_PC_CLOCK, lj_cl_hmsf_ms(0, 0, 10, 0, 100), 0, 4, 0);
    rows[15] = t_line(0, LJ_CL_WAIT_ELAPSED, 1000, 0, 1, 0);
    rows[16] = t_line(0, LJ_CL_GOTO_LINE, 19, 0, 0, 0);
    rows[17] = t_line(0, LJ_CL_NONE, 0, 0, 4, 0);
    rows[18] = t_line(0, LJ_CL_GOTO_LINE, 2, 0, 0, 0);
    rows[19] = t_line(0, LJ_CL_GOTO_LINE, 99, 0, 0, 0);
    memset(cl, 0, sizeof *cl);
    cl->number = 1; cl->mark[0] = 9; cl->mark[1] = 20;
    for (i = 0; i < 20; i++) lj_cuelist_insert(cl, (uint32_t)i, &rows[i]);
}

static long t_nth(char kind, uint32_t a, int nth) { int i = -1; while (nth-- > 0) if ((i = t_find(kind, a, i + 1)) < 0) return -1; return (long)(T.ev[i].t / 1000u); }   /* ms of the nth such event */

/* W3 acceptance 1-2: the 20-line list against PC elapsed time */
static void t_acceptance(void)
{
    lj_cuelist cl;
    int i;
    t_list20(&cl);
    CHECK(lj_cuelist_validate(&cl) == NULL);
    t_reset(0);
    CHECK(t_op(&cl, LJ_CL_OP_TOP, 0) == 0);
    CHECK(t_at('c', 1) == 0 && R.st.state == LJ_CL_RUNNING && R.st.next == 2 && R.st.last == 1);
    t_run_to(990);  CHECK(t_at('c', 2) == -1);
    t_run_to(1000); CHECK(t_at('c', 2) == 1000);                              /* Wait Elapsed 1.00 s */
    CHECK(R.st.state == LJ_CL_WAIT_GO && R.st.next == 3);                     /* manual line 3 */
    t_run_to(6000); CHECK(t_at('c', 3) == -1 && R.st.state == LJ_CL_WAIT_GO); /* its 5 s wait is not what moves it */
    CHECK(lj_cl_line_state(&R.st, &cl, 3) == LJ_CLL_NEXT && lj_cl_line_state(&R.st, &cl, 2) == LJ_CLL_DONE && lj_cl_line_state(&R.st, &cl, 5) == LJ_CLL_PLAIN);   /* W5: MP is live */
    t_reset(0);                                                               /* again, Go after 2 s */
    t_op(&cl, LJ_CL_OP_TOP, 0);
    t_run_to(2000); CHECK(t_op(&cl, LJ_CL_OP_GO, 0) == 0 && t_at('c', 3) == 2000);   /* Go !: the wait ignored */
    t_run_to(2490); CHECK(t_count('t') == 0);
    t_run_to(2500);
    i = t_find('t', LJ_RUN_SLOT0 + 0, 0);
    CHECK(i >= 0 && T.ev[i].t == 2500000u && T.ev[i].b == 0xFF && t_find('t', LJ_RUN_SLOT0 + 1, 0) == i + 1 && t_count('t') == 2);   /* Set Cue Trig 1+2, once each */
    CHECK(t_at('m', LJ_CL_MP_PLAY) == 2500 && t_at('c', 4) == 2500 && t_at('h', 7) == 2500);   /* W5: the MP line executes and latches its cue, Hotkey Command right after */
    t_run_to(2990); CHECK(t_count('f') == 0);
    t_run_to(3000); CHECK(t_at('f', 5) == 3000 && T.ev[t_find('f', 5, 0)].b == 1);   /* Flash Seq 5 */
    t_run_to(3490); CHECK(t_count('f') == 1);
    t_run_to(3500); i = t_find('f', 5, t_find('f', 5, 0) + 1); CHECK(i >= 0 && T.ev[i].b == 0 && T.ev[i].t == 3500000u);   /* released after 0.50 s */
    t_run_to(3990); CHECK(t_nth('c', 4, 2) == -1);
    t_run_to(4000); CHECK(t_nth('c', 4, 2) == 4000 && t_at('x', 0) == 4000 && t_at('g', 1) == 4000);   /* cue 4; Execute refused; Load BGCue 1 */
    CHECK(R.st.state == LJ_CL_WAIT_GO && R.st.next == 12);
    t_run_to(6000); CHECK(t_op(&cl, LJ_CL_OP_GO, 0) == 0 && t_nth('c', 1, 2) == 6000 && R.st.state == LJ_CL_RUNNING && R.st.next == 13);
    t_run_to(7990); CHECK(R.st.next == 13 && t_nth('c', 2, 2) == -1);        /* Wait MTC, no time code: waits */
    t_run_to(8000); CHECK(t_op(&cl, LJ_CL_OP_GO, 0) == 0 && t_nth('c', 2, 2) == 8000 && R.st.next == 14);   /* until Go */
    t_run_to(9000); CHECK(t_nth('c', 3, 2) == 9000);
    t_run_to(9990); CHECK(R.st.next == 15 && t_nth('c', 4, 3) == -1);
    t_run_to(10000); CHECK(t_nth('c', 4, 3) == 10000 && R.st.next == 16);    /* Wait PC-Clock 00:00:10 */
    t_run_to(11000); CHECK(t_nth('c', 1, 3) == 11000 && R.st.last == 19 && R.st.next == 2);   /* Goto 19 over line 18, Goto 2 */
    CHECK(t_count('c') == 10);                                                /* 1 2 3 4 4 1 2 3 4 1 (W5: line 5's cue 4): line 18's cue 4 never */
    t_run_to(11990); CHECK(t_nth('c', 2, 3) == -1);
    t_run_to(12000); CHECK(t_nth('c', 2, 3) == 12000 && R.st.state == LJ_CL_WAIT_GO && R.st.next == 3);
    CHECK(R.st.elapsed_ms == 12000 && R.st.tc_ms == 12000 && R.st.number == 1);
    CHECK(t_op(&cl, LJ_CL_OP_STOP, 0) == 0 && R.st.state == LJ_CL_IDLE && R.st.next == 0);
    free(cl.lines);
}

/* Goto beyond the end, Back, Jump, marks, Run to line, tracking, Link, Stop and Clear, clock / date / MTC, Log */
static void t_ops(void)
{
    lj_cuelist cl, other;
    lj_cl_line l;
    t_list20(&cl);
    /* Goto Line# 99 at line 20 ends the list (RN v2.6 b7: ignored, never a stall) */
    t_reset(0);
    CHECK(t_op(&cl, LJ_CL_OP_RUN, 20) == 0 && R.st.state == LJ_CL_IDLE && R.st.last == 20);
    /* Run from line 14, tracking off: nothing before it latches; on: lines 1..13's cues in order */
    t_reset(0);
    CHECK(t_op(&cl, LJ_CL_OP_RUN, 14) == 0 && t_count('c') == 0);
    t_reset(0); cl.tracking = 1;
    CHECK(t_op(&cl, LJ_CL_OP_RUN, 14) == 0 && t_count('c') == 7 && T.ev[0].a == 1 && T.ev[1].a == 2 && T.ev[2].a == 3 && T.ev[3].a == 4 && T.ev[4].a == 4 && T.ev[5].a == 1 && T.ev[6].a == 2);   /* W5: the MP line's cue 4 too */
    cl.tracking = 0;
    /* Jump while waiting for Go: tracking off skips the cues before the target */
    t_reset(0);
    t_op(&cl, LJ_CL_OP_TOP, 0); t_run_to(1000);
    CHECK(R.st.state == LJ_CL_WAIT_GO && t_op(&cl, LJ_CL_OP_JUMP, 12) == 0 && R.st.next == 12 && t_count('c') == 2);
    CHECK(t_op(&cl, LJ_CL_OP_GO, 0) == 0 && T.ev[T.n - 1].a == 1 && R.st.next == 13);
    CHECK(t_op(&cl, LJ_CL_OP_JUMP, 3) == -1);                                 /* only while waiting for Go */
    CHECK(t_op(&cl, LJ_CL_OP_BACK, 0) == 0 && R.st.next == 12 && R.st.state == LJ_CL_WAIT_GO);   /* Back: one line up */
    /* marks: Goto Mark 1 (line 9) while running; Run from Mark 2 (line 20) ends at once */
    CHECK(t_op(&cl, LJ_CL_OP_MARK_GOTO, 1) == 0 && R.st.next == 9 && R.st.state == LJ_CL_RUNNING);
    CHECK(t_op(&cl, LJ_CL_OP_MARK_GOTO, 3) == -1);                            /* mark 3 not set */
    CHECK(t_op(&cl, LJ_CL_OP_MARK_RUN, 2) == 0 && R.st.state == LJ_CL_IDLE);
    CHECK(t_op(&cl, LJ_CL_OP_BACK, 0) == -1 && t_op(&cl, LJ_CL_OP_MARK_GOTO, 1) == -1);   /* idle */
    /* Run to line 9 while idle: only the cue commands up to and including it [RN v2.5 b3] */
    t_reset(0);
    CHECK(t_op(&cl, LJ_CL_OP_RUN_TO, 9) == 0 && t_count('c') == 5 && T.ev[3].a == 4 && T.ev[4].a == 4 && t_count('h') == 0 && t_count('f') == 0 && t_count('t') == 0);
    CHECK(R.st.state == LJ_CL_IDLE && R.st.last == 9);
    /* Go on an idle list = Run from top [03 s6.5]; Stop and Clear clears the cue */
    t_reset(0);
    CHECK(t_op(&cl, LJ_CL_OP_GO, 0) == 0 && t_at('c', 1) == 0 && R.st.state == LJ_CL_RUNNING);
    CHECK(t_op(&cl, LJ_CL_OP_STOP_CLEAR, 0) == 0 && R.st.state == LJ_CL_IDLE && t_count('z') == 1);
    CHECK(t_op(&cl, LJ_CL_OP_TOP, 0) == 0 && t_op(&cl, LJ_CL_OP_RUN, 21) == -1);
    /* a cue that fails to load is logged, the list goes on */
    t_reset(0);
    memset(&other, 0, sizeof other); other.number = 2;
    l = t_line(0, LJ_CL_NONE, 0, 0, 404, 0); lj_cuelist_insert(&other, 0, &l);
    l = t_line(0, LJ_CL_NONE, 0, 0, 5, 0); lj_cuelist_insert(&other, 1, &l);
    CHECK(t_op(&other, LJ_CL_OP_TOP, 0) == 0 && t_at('c', 404) == 0 && t_at('c', 5) == 0 && R.st.state == LJ_CL_IDLE);
    /* Link Cuelist: the run continues at the linked list's line 1 */
    t_reset(0);
    l = t_line(0, LJ_CL_LINK_CUELIST, 0, 2, 0, 0);
    {
        lj_cuelist one; memset(&one, 0, sizeof one); one.number = 3;
        lj_cuelist_insert(&one, 0, &l);
        T.linked = &other;
        CHECK(t_op(&one, LJ_CL_OP_TOP, 0) == 0 && t_at('k', 2) == 0 && t_at('c', 5) == 0 && R.st.number == 2 && R.st.state == LJ_CL_IDLE);
        T.linked = NULL; t_reset(0);
        CHECK(t_op(&one, LJ_CL_OP_TOP, 0) == 0 && R.st.number == 3 && R.st.state == LJ_CL_IDLE);   /* no such list: logged, ends */
        lj_cl_run_unpin(&R, &one);
        free(one.lines);
    }
    /* Wait PC-Clock across midnight and Wait Date-Change: the clock starts at 23:59:59.000 */
    {
        lj_cuelist c2; memset(&c2, 0, sizeof c2); c2.number = 4;
        l = t_line(0, LJ_CL_WAIT_PC_CLOCK, lj_cl_hmsf_ms(0, 0, 1, 0, 100), 0, 6, 0); lj_cuelist_insert(&c2, 0, &l);
        l = t_line(0, LJ_CL_WAIT_DATE_CHANGE, 0, 0, 7, 0); lj_cuelist_insert(&c2, 1, &l);
        t_reset(86399000u);
        CHECK(t_op(&c2, LJ_CL_OP_TOP, 0) == 0 && t_count('c') == 0);
        t_run_to(1990); CHECK(t_count('c') == 0);
        t_run_to(2000); CHECK(t_at('c', 6) == 2000);                          /* 00:00:01 the next day */
        t_run_to(5000); CHECK(t_at('c', 7) == -1 && R.st.next == 2);          /* the date-change wait began after midnight */
        t_reset(86399000u);
        lj_cuelist_delete(&c2, 0);
        CHECK(t_op(&c2, LJ_CL_OP_TOP, 0) == 0);
        t_run_to(990); CHECK(t_at('c', 7) == -1);
        t_run_to(1000); CHECK(t_at('c', 7) == 1000);                          /* midnight */
        /* MTC: a time code that arrives satisfies the wait; the shown source and Log time code */
        t_reset(0);
        l = t_line(0, LJ_CL_WAIT_MTC, 5000, 0, 8, 0); lj_cuelist_insert(&c2, 0, &l);
        R.st.tc_src = LJ_TC_MIDI;
        CHECK(t_op(&c2, LJ_CL_OP_TOP, 0) == 0);
        t_run_to(100); CHECK(t_at('c', 8) == -1 && R.st.tc_ms == -1 && t_op(&c2, LJ_CL_OP_LOG, 0) == -1);
        T.mtc_ok = 1; T.mtc_ms = 4990;
        t_run_to(110); CHECK(t_at('c', 8) == -1 && R.st.tc_ms == 4990 && t_op(&c2, LJ_CL_OP_LOG, 0) == 0 && R.st.nlog == 1 && R.st.log_ms[0] == 4990);
        T.mtc_ms = 5000;
        t_run_to(120); CHECK(t_at('c', 8) == 120);
        free(c2.lines);
    }
    /* Ignore first Wait Elapsed on Run */
    t_reset(0); R.ignore_first_wait = 1;
    CHECK(t_op(&cl, LJ_CL_OP_RUN, 2) == 0 && t_at('c', 2) == 0 && R.st.state == LJ_CL_WAIT_GO);
    /* unpin: the app frees the list it runs */
    lj_cl_run_unpin(&R, &cl);
    CHECK(R.st.state == LJ_CL_IDLE && R.cl == NULL && lj_cl_run_step(&R, T.now + 10000u, &H) == 0);
    /* a Goto loop with no waits runs one round per pass, never spins */
    {
        lj_cuelist lp; memset(&lp, 0, sizeof lp); lp.number = 5;
        l = t_line(0, LJ_CL_NONE, 0, 0, 9, 0); lj_cuelist_insert(&lp, 0, &l);
        l = t_line(0, LJ_CL_GOTO_LINE, 1, 0, 0, 0); lj_cuelist_insert(&lp, 1, &l);
        t_reset(0);
        CHECK(t_op(&lp, LJ_CL_OP_TOP, 0) == 0 && t_count('c') == 2);
        CHECK(lj_cl_run_step(&R, 10000u, &H) == 3 && R.st.state == LJ_CL_RUNNING);
        free(lp.lines);
    }
    /* ME-108 row A: line 1 Hotkey Command = Cuelist Stop: the list stops, cue 2 of line 2 never loads */
    {
        lj_cuelist re; memset(&re, 0, sizeof re); re.number = 6;
        l = t_line(0, LJ_CL_HOTKEY_COMMAND, 7, 0, 0, 0); lj_cuelist_insert(&re, 0, &l);
        l = t_line(0, LJ_CL_NONE, 0, 0, 2, 0); lj_cuelist_insert(&re, 1, &l);
        t_reset(0); memset(&RE, 0, sizeof RE); RE.op = LJ_CL_OP_STOP;
        CHECK(t_op(&re, LJ_CL_OP_TOP, 0) == 0 && R.st.state == LJ_CL_IDLE && t_at('c', 2) == -1 && R.st.last == 1);
        t_run_to(100); CHECK(t_at('c', 2) == -1);
        /* row B: Cuelist Go from inside the line returns at depth 1 (-1), never recurses */
        t_reset(0); memset(&RE, 0, sizeof RE); RE.op = LJ_CL_OP_GO;
        CHECK(t_op(&re, LJ_CL_OP_TOP, 0) == 0 && RE.max_depth == 1 && RE.rc == -1 && t_count('h') == 1 && t_at('c', 2) == 0 && R.st.state == LJ_CL_IDLE);
        memset(&RE, 0, sizeof RE);
        free(re.lines);
    }
    /* ME-116: the time code drops out at a Wait MTC: Stop (default) / Restart / Ignore */
    {
        lj_cuelist mt; int mode;
        memset(&mt, 0, sizeof mt); mt.number = 7;
        l = t_line(0, LJ_CL_NONE, 0, 0, 1, 0); lj_cuelist_insert(&mt, 0, &l);
        l = t_line(0, LJ_CL_WAIT_MTC, 5000, 0, 2, 0); lj_cuelist_insert(&mt, 1, &l);
        for (mode = LJ_CL_MTC_STOP; mode <= LJ_CL_MTC_IGNORE; mode++) {
            t_reset(0); R.mtc_timeout = (uint8_t)mode;
            CHECK(t_op(&mt, LJ_CL_OP_TOP, 0) == 0 && R.st.next == 2);
            T.mtc_ok = 1; T.mtc_ms = 1000; t_run_to(100);
            CHECK(R.st.state == LJ_CL_RUNNING && R.st.next == 2 && t_count('c') == 1);
            T.mtc_ok = 0; t_run_to(110);                                         /* lj_mtc_time is -1 once the code is 500 ms old */
            if (mode == LJ_CL_MTC_STOP) CHECK(R.st.state == LJ_CL_IDLE);
            if (mode == LJ_CL_MTC_RESTART) CHECK(R.st.state == LJ_CL_RUNNING && R.st.next == 2 && t_count('c') == 2 && T.ev[1].a == 1);
            if (mode == LJ_CL_MTC_IGNORE) CHECK(R.st.state == LJ_CL_RUNNING && R.st.next == 2 && t_count('c') == 1);
            t_run_to(1000); CHECK(t_count('c') == (mode == LJ_CL_MTC_RESTART ? 2 : 1));   /* one edge, one action */
        }
        free(mt.lines);
    }
    /* ME-171 (f): three Wait Elapsed 1.00 s lines on a 15.6 ms wake (the default Windows timer): each counts from the
     * previous one's due time, so line 3 fires at the first wake after 3000 ms (3010.8), not 3042 (the drift of two wakes) */
    {
        lj_cuelist we; uint32_t k;
        memset(&we, 0, sizeof we); we.number = 8;
        for (k = 0; k < 3; k++) { l = t_line(0, LJ_CL_WAIT_ELAPSED, 1000, 0, k + 1, 0); lj_cuelist_insert(&we, k, &l); }
        t_reset(0);
        CHECK(t_op(&we, LJ_CL_OP_TOP, 0) == 0);
        while (T.now < 4000000u) { T.now += 15600u; lj_cl_run_step(&R, T.now, &H); }
        CHECK(t_at('c', 1) == 1014 && t_at('c', 2) == 2012 && t_at('c', 3) == 3010);
        if (t_at('c', 3) != 3010) printf("  Wait Elapsed fired at %ld %ld %ld ms\n", t_at('c', 1), t_at('c', 2), t_at('c', 3));
        free(we.lines);
    }
    /* ME-171 (g): Run from line 6 with Automatic Cue update: line 3 (cue 3) is the last non-transparent cue before it,
     * so only cues 3, 104, 105 load (not 1 and 2), then line 6's own cue 6 */
    {
        lj_cuelist ac; static const uint32_t cues[6] = { 1, 2, 3, 104, 105, 6 }; uint32_t k;
        memset(&ac, 0, sizeof ac); ac.number = 9;
        for (k = 0; k < 6; k++) { l = t_line(0, LJ_CL_NONE, 0, 0, cues[k], 0); lj_cuelist_insert(&ac, k, &l); }
        t_reset(0); R.auto_cue_on_run = 1;
        CHECK(t_op(&ac, LJ_CL_OP_RUN, 6) == 0);
        CHECK(t_count('c') == 4 && T.ev[0].a == 3 && T.ev[1].a == 104 && T.ev[2].a == 105 && T.ev[3].a == 6 && t_at('c', 1) == -1);
        free(ac.lines);
    }
    free(cl.lines); free(other.lines);
}

/* LORD_ENGINE-7 and -9 (W2 verdict s6): the engine rows the cue list now exercises */
static void t_engine_rows(void)
{
    static lj_sequence q; static lj_scene sc[2];
    lj_show *show = (lj_show *)calloc(1, sizeof *show);
    lj_engine *e = lj_engine_create();
    lj_engine_status v;
    lj_cmd c;
    uint64_t now = 0;
    int k, kind;
    if (!show || !e) { CHECK(0); free(show); lj_engine_destroy(e); return; }
    lj_engine_bind(e, show);
    for (kind = 0; kind < 3; kind++) {                                         /* GO / STOP / TRIG on the first static runner and the last id: no-op, no crash */
        memset(&c, 0, sizeof c);
        c.kind = (uint8_t)(kind == 0 ? LJ_CMD_GO : kind == 1 ? LJ_CMD_STOP : LJ_CMD_TRIG); c.b = 0xFF;
        c.a = LJ_RUN_STATIC0; CHECK(lj_engine_cmd(e, &c) == 0);
        c.a = LJ_RUN_COUNT - 1; CHECK(lj_engine_cmd(e, &c) == 0);
    }
    lj_engine_tick(e, now); now += 25000u;
    lj_engine_view(e, &v);
    for (k = 0; k < LJ_CHANS; k++) if (lj_engine_frame(e)->merged[k]) { CHECK(0); break; }
    /* LORD_ENGINE-9: a held current scene (Stop), frozen for 5 s: the scene span stays its 1000 ms, never a wrapped value */
    memset(&q, 0, sizeof q); memset(sc, 0, sizeof sc);
    q.number = 1; q.n = 2; q.scenes = sc;
    for (k = 0; k < 2; k++) { sc[k].scene_ms = 1000; sc[k].fade_ms = 50; sc[k].val[0] = (uint8_t)(10 + k); sc[k].state[0] = LJ_SNAP; }
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_CURRENT_SEQ; c.seq[0] = &q; lj_engine_cmd(e, &c);
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_STOP; c.a = LJ_RUN_CURRENT; lj_engine_cmd(e, &c);
    lj_engine_tick(e, now); now += 25000u;
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_FREEZE; c.b = 1; lj_engine_cmd(e, &c);
    lj_engine_tick(e, now);
    now += 5000000u;
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_FREEZE; c.b = 0; lj_engine_cmd(e, &c);
    lj_engine_tick(e, now);
    lj_engine_view(e, &v);
    CHECK(v.cur_scene_ms <= 1000 && v.cur_scene_elapsed_ms <= v.cur_scene_ms);
    if (v.cur_scene_ms > 1000) printf("  cur_scene_ms %u after the unfreeze\n", (unsigned)v.cur_scene_ms);
    lj_engine_bind(e, NULL); lj_engine_destroy(e); free(show);
}

int lj_test_cuelist_run(int argc, char **argv);
int lj_test_cuelist_run(int argc, char **argv)
{
    (void)argc; (void)argv;
    t_acceptance();
    t_ops();
    t_engine_rows();
    printf("test_cuelist_run: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
