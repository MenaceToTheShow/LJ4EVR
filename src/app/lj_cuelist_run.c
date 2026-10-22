/* lj_cuelist_run.c - the cue list runner (lj_cuelist_run.h; design 2.3 rule 4, 03 s6). UI thread only, SDL-free, no
 * I/O: every effect goes out through the lj_cl_host. One lj_cl_run_step per 10 ms UI wake, the time read once per
 * pass (now_us, one clock read, one time code read). */
#include <string.h>
#include "app/lj_cuelist_run.h"
#include "core/lj_log.h"

enum { LJ_CL_DAY_MS = 86400000, LJ_CL_CLOCK_WINDOW_MS = 1000 };   /* Wait PC-Clock fires inside 1 s after its time (H:M:S resolution); PROPOSAL rig 03 s6.3: LJ2's comparison is not described */

static void lj_cl_changed(lj_cl_run *r) { r->st.serial++; }

static void lj_cl_flash_off(lj_cl_run *r, const lj_cl_host *h)
{
    if (r->flash_seq && h->flash) h->flash(h->ud, r->flash_seq, 0);
    r->flash_seq = 0;
}

/* line n (1-based) becomes the waiting one; a Wait Date-Change line samples the day it began waiting */
static void lj_cl_arrive(lj_cl_run *r, uint32_t n, uint32_t day)
{
    r->st.next = n;
    if (n >= 1 && n <= r->cl->n && r->cl->lines[n - 1].cmd == LJ_CL_WAIT_DATE_CHANGE) r->day0 = day;
}

static void lj_cl_end(lj_cl_run *r)   /* the list ran out or STOP; a Flash Sequence line still times out (step) */
{
    r->st.state = LJ_CL_IDLE;
    r->st.next = 0;
    lj_cl_changed(r);
}

/* the cue latch or the slot trigs of a line (03 s6.1: one or the other) */
static void lj_cl_latch(const lj_cl_line *l, const lj_cl_host *h)
{
    int k;
    if (l->cue) { if (!h->load_cue || h->load_cue(h->ud, l->cue) < 0) lj_log("cuelist: line %u: cue %u not loaded", (unsigned)l->line, (unsigned)l->cue); return; }
    for (k = 0; k < LJ_CUE_SLOTS; k++) {
        lj_cmd c;
        if (!(l->trig_mask & (1u << k)) || !h->post) continue;
        memset(&c, 0, sizeof c);
        c.kind = LJ_CMD_TRIG; c.a = (uint32_t)(LJ_RUN_SLOT0 + k); c.b = 0xFF;   /* "trigged exactly once ... in the programmed loop direction" [03 s6.2] */
        h->post(h->ud, &c);
    }
}

/* Run to line / tracking / Automatic Cue update: only the cue commands of lines 1..n, in order [RN v2.5 b3]; each
 * cue load layers like the list would have (a transparent cue keeps what the cue before it left). */
static void lj_cl_track(const lj_cuelist *cl, uint32_t n, const lj_cl_host *h)
{
    uint32_t i;
    for (i = 0; i < n && i < cl->n; i++)
        if (cl->lines[i].cue && !cl->lines[i].load_error) lj_cl_latch(&cl->lines[i], h);
}

static int lj_cl_wait_holds(lj_cl_run *r, const lj_cl_line *l, uint64_t now, int clk_ok, uint32_t ms_of_day, uint32_t day, int mtc_ok, uint32_t mtc_ms)
{
    switch (l->cmd) {
    case LJ_CL_WAIT_ELAPSED:
        if (r->ignore_first_wait && r->first_pending) return 1;               /* M_IgnoreFirstWait */
        return now - r->line_t0_us >= (uint64_t)l->param_ms * 1000u;
    case LJ_CL_WAIT_PC_CLOCK:
        return clk_ok && (ms_of_day + LJ_CL_DAY_MS - l->param_ms) % LJ_CL_DAY_MS < LJ_CL_CLOCK_WINDOW_MS;
    case LJ_CL_WAIT_DATE_CHANGE:
        return clk_ok && day != r->day0;
    case LJ_CL_WAIT_MTC:
        return mtc_ok && mtc_ms >= l->param_ms;                                /* no time code = waits until Go (LJ2 with no MTC) */
    default:
        return 1;
    }
}

/* Executes line r->st.next: moves next on, then the command and the latch. 0 = the pass goes on, 1 = it stops (the list
 * ended, or a host callback re-entered the runner: Hotkey Command = Cuelist Stop / Top / Prog, ME-108). st.last,
 * line_t0_us and next are committed BEFORE any callback that can re-enter, and in_exec keeps a nested op from running
 * a pass of its own (Cuelist Go from inside a line returns -1 instead of recursing). */
static int lj_cl_exec(lj_cl_run *r, uint64_t now, uint32_t day, int mtc_ok, uint32_t mtc_ms, const lj_cl_host *h)
{
    const lj_cl_line l = r->cl->lines[r->st.next - 1];   /* a copy: Link Cuelist may hand the old list back to its owner */
    const lj_cuelist *cl0;
    uint32_t next = r->st.next + 1, i;
    if (l.cmd == LJ_CL_WAIT_ELAPSED) r->first_pending = 0;
    if (l.cmd == LJ_CL_GOTO_LINE) next = (l.param_ms < 1 || l.param_ms > r->cl->n) ? 0 : l.param_ms;   /* beyond the last line: ignored, the list ends (LJ_CL_GOTO_BEYOND_IGNORED) */
    if (l.cmd == LJ_CL_AUTOSEEK_MTC && mtc_ok)                                 /* jump to the first Wait MTC still ahead of the time code; none = go on */
        for (i = 0; i < r->cl->n; i++)
            if (r->cl->lines[i].cmd == LJ_CL_WAIT_MTC && !r->cl->lines[i].load_error && r->cl->lines[i].param_ms >= mtc_ms) { next = i + 1; break; }
    r->st.last = l.line;
    r->line_t0_us = now;
    if (l.cmd == LJ_CL_LINK_CUELIST) {                                         /* the host loads the list; it never re-enters the runner */
        const lj_cuelist *nl = NULL;
        if (h->link && h->link(h->ud, l.ref, &nl) == 0 && nl && nl->n) { r->cl = nl; r->st.number = nl->number; r->st.last = 0; next = 1; }
        else lj_log("cuelist: line %u: cue list %u not linked", (unsigned)l.line, (unsigned)l.ref);
    }
    if (!next || next > r->cl->n) lj_cl_end(r);
    else { lj_cl_arrive(r, next, day); lj_cl_changed(r); }
    cl0 = r->cl;
    r->in_exec = 1;
    switch (l.cmd) {
    case LJ_CL_EXECUTE_FILE:
        if (!h->exec || h->exec(h->ud, l.param, l.comment) < 0) lj_log("cuelist: line %u: Execute File refused or failed ([CUELIST2] AllowExecute)", (unsigned)l.line);
        break;
    case LJ_CL_HOTKEY_COMMAND:
        if (h->fn) h->fn(h->ud, (int)l.param_ms);
        break;
    case LJ_CL_FLASH_SEQUENCE:
        lj_cl_flash_off(r, h);                                                 /* "only one sequence may be flashed at any given time" */
        if (h->flash && h->flash(h->ud, l.ref, 1) == 0) { r->flash_seq = l.ref; r->flash_off_us = now + (uint64_t)l.param_ms * 1000u; }
        else lj_log("cuelist: line %u: sequence %u not flashed", (unsigned)l.line, (unsigned)l.ref);
        break;
    case LJ_CL_LOAD_BACKGROUND_CUE:
        if (!h->load_bg || h->load_bg(h->ud, l.ref) < 0) lj_log("cuelist: line %u: background cue %u not loaded", (unsigned)l.line, (unsigned)l.ref);
        break;
    default: break;
    }
    lj_cl_latch(&l, h);
    r->in_exec = 0;
    return r->st.state == LJ_CL_IDLE || !r->cl || r->cl != cl0;
}

static int lj_cl_clock(const lj_cl_host *h, uint32_t *ms_of_day, uint32_t *day)
{
    *ms_of_day = 0; *day = 0;
    return h->clock && h->clock(h->ud, ms_of_day, day) == 0;
}

static int lj_cl_start(lj_cl_run *r, const lj_cuelist *cl, uint32_t n, uint64_t now, const lj_cl_host *h, int track);

static int lj_cl_pass(lj_cl_run *r, uint64_t now, const lj_cl_host *h, int force)
{
    if (r->in_exec) return 0;                                                   /* ME-108: an op from inside a line never runs a nested pass */
    uint32_t ms_of_day, day, mtc_ms = 0, seen;
    int clk_ok = lj_cl_clock(h, &ms_of_day, &day), mtc_ok = h->timecode && h->timecode(h->ud, LJ_TC_MIDI, &mtc_ms) == 0, done = 0;
    uint32_t tc = 0;
    if (r->flash_seq && now >= r->flash_off_us) lj_cl_flash_off(r, h);
    if (r->st.state != LJ_CL_IDLE) r->st.elapsed_ms = (uint32_t)((now - r->run_t0_us) / 1000u);
    switch (r->st.tc_src) {
    case LJ_TC_PC_ELAPSED: r->st.tc_ms = r->st.state != LJ_CL_IDLE ? (int32_t)r->st.elapsed_ms : -1; break;
    case LJ_TC_PC_CLOCK:   r->st.tc_ms = clk_ok ? (int32_t)ms_of_day : -1; break;
    case LJ_TC_MIDI:       r->st.tc_ms = mtc_ok ? (int32_t)mtc_ms : -1; break;
    default:               r->st.tc_ms = h->timecode && h->timecode(h->ud, r->st.tc_src, &tc) == 0 ? (int32_t)tc : -1; break;   /* CD / DA / MP: -1 in W3 */
    }
    if (r->mtc_seen && !mtc_ok && r->st.state != LJ_CL_IDLE && r->cl && r->st.next >= 1 && r->st.next <= r->cl->n
        && r->cl->lines[r->st.next - 1].cmd == LJ_CL_WAIT_MTC) {               /* ME-116: the time code dropped out (LJ_MTC_TIMEOUT_MS) at a Wait MTC */
        r->mtc_seen = 0;
        if (r->mtc_timeout == LJ_CL_MTC_STOP) { lj_cl_end(r); return 0; }
        if (r->mtc_timeout == LJ_CL_MTC_RESTART) { lj_cl_start(r, r->cl, 1, now, h, 0); return 0; }
    }
    r->mtc_seen = (uint8_t)mtc_ok;
    if (r->st.state == LJ_CL_IDLE || !r->cl) return 0;
    for (seen = 0; seen <= r->cl->n; seen++) {                                  /* a Goto loop with no waits runs one round per pass, never spins */
        const lj_cl_line *l;
        if (r->st.next < 1 || r->st.next > r->cl->n) { lj_cl_end(r); break; }
        l = &r->cl->lines[r->st.next - 1];
        if (l->load_error || lj_cl_cmd_inert(l->cmd)) {                        /* design 8.5: skipped as if absent */
            lj_log("cuelist: line %u skipped (load error: %s)", (unsigned)l->line, l->cmd ? lj_cl_cmd_caption[l->cmd] : l->param);
            if (r->st.next == r->cl->n) { lj_cl_end(r); break; }
            lj_cl_arrive(r, r->st.next + 1, day); lj_cl_changed(r);
            continue;
        }
        if (!force) {
            if (l->manual) {                                                   /* every Wait ignored, Go ! advances (LJ_CL_MANUAL_IGNORES_WAITS) */
                if (r->st.state != LJ_CL_WAIT_GO) { r->st.state = LJ_CL_WAIT_GO; lj_cl_changed(r); }
                break;
            }
            if (!lj_cl_wait_holds(r, l, now, clk_ok, ms_of_day, day, mtc_ok, mtc_ms)) {
                if (r->st.state != LJ_CL_RUNNING) { r->st.state = LJ_CL_RUNNING; lj_cl_changed(r); }
                break;
            }
        }
        force = 0;
        r->st.state = LJ_CL_RUNNING;
        done++;
        if (lj_cl_exec(r, now, day, mtc_ok, mtc_ms, h)) break;
    }
    return done;
}

void lj_cl_run_init(lj_cl_run *r)
{
    memset(r, 0, sizeof *r);
    r->st.tc_src = LJ_TC_PC_ELAPSED;
    r->st.tc_ms = -1;
}

static int lj_cl_start(lj_cl_run *r, const lj_cuelist *cl, uint32_t n, uint64_t now, const lj_cl_host *h, int track)
{
    uint32_t ms, day;
    if (!cl || !cl->n || n < 1 || n > cl->n) return -1;
    lj_cl_flash_off(r, h);
    if (track && n > 1) lj_cl_track(cl, n - 1, h);
    lj_cl_clock(h, &ms, &day);
    r->cl = cl; r->st.number = cl->number; r->st.last = 0;
    r->run_t0_us = r->line_t0_us = now; r->st.elapsed_ms = 0;
    r->first_pending = 1;
    r->st.state = LJ_CL_RUNNING;
    lj_cl_arrive(r, n, day);
    lj_cl_changed(r);
    lj_cl_pass(r, now, h, 0);
    return 0;
}

/* jump while running: tracking on latches the cues up to the line before it (PROPOSAL rig 03 s6, header comment) */
static int lj_cl_jump(lj_cl_run *r, uint32_t n, uint64_t now, const lj_cl_host *h)
{
    uint32_t ms, day;
    if (r->st.state == LJ_CL_IDLE || !r->cl || n < 1 || n > r->cl->n) return -1;
    if (r->cl->tracking && n > 1) lj_cl_track(r->cl, n - 1, h);
    lj_cl_clock(h, &ms, &day);
    r->line_t0_us = now;
    lj_cl_arrive(r, n, day);
    lj_cl_changed(r);
    lj_cl_pass(r, now, h, 0);
    return 0;
}

int lj_cl_run_op(lj_cl_run *r, const lj_cuelist *cl, int op, uint32_t n, uint64_t now_us, const lj_cl_host *h)
{
    const lj_cuelist *run = r->st.state != LJ_CL_IDLE && r->cl ? r->cl : cl;
    uint32_t mark;
    switch (op) {
    case LJ_CL_OP_TOP: return lj_cl_start(r, cl, 1, now_us, h, 0);
    case LJ_CL_OP_RUN: return lj_cl_start(r, cl, n ? n : 1, now_us, h, r->auto_cue_on_run || (cl && cl->tracking));
    case LJ_CL_OP_GO:
        if (r->in_exec) return -1;                                             /* ME-108: Hotkey Command = Cuelist Go from inside a line */
        if (r->st.state == LJ_CL_IDLE) return lj_cl_start(r, cl, 1, now_us, h, 0);   /* "if not running, acts as Run from top" [03 s6.5] */
        /* Go executes the waiting line: a manual line, or an auto line held by its wait (an MTC wait with no time code);
         * the second half is PROPOSAL rig 03 s6.2 (LJ2 documents Go for manual lines only) */
        return lj_cl_pass(r, now_us, h, 1) > 0 ? 0 : -1;
    case LJ_CL_OP_BACK:
        if (r->st.state == LJ_CL_IDLE || r->st.next <= 1) return -1;           /* "not on the first line" */
        return lj_cl_jump(r, r->st.next - 1, now_us, h);
    case LJ_CL_OP_STOP: case LJ_CL_OP_STOP_CLEAR:
        lj_cl_flash_off(r, h);
        if (r->st.state != LJ_CL_IDLE) lj_cl_end(r);
        if (op == LJ_CL_OP_STOP_CLEAR && h->clear_cue) h->clear_cue(h->ud);
        return 0;
    case LJ_CL_OP_JUMP:
        if (r->st.state != LJ_CL_WAIT_GO) return -1;                           /* Pop_JumpToThis: while waiting for Go [RN v2.6 b7] */
        return lj_cl_jump(r, n, now_us, h);
    case LJ_CL_OP_RUN_TO:
        if (!run || n < 1 || n > run->n) return -1;
        lj_cl_track(run, n, h);
        r->st.last = n;
        if (r->st.state != LJ_CL_IDLE) { if (n == run->n) lj_cl_end(r); else { r->line_t0_us = now_us; r->st.next = n + 1; } }
        lj_cl_changed(r);
        return 0;
    case LJ_CL_OP_MARK_GOTO: case LJ_CL_OP_MARK_RUN:
        if (!run || n < 1 || n > LJ_MARKS || !(mark = run->mark[n - 1])) return -1;
        if (op == LJ_CL_OP_MARK_RUN) return lj_cl_start(r, run, mark, now_us, h, r->auto_cue_on_run || run->tracking);
        return lj_cl_jump(r, mark, now_us, h);                                 /* idle: the window only selects the line */
    case LJ_CL_OP_LOG:
        if (r->st.tc_ms < 0 || r->st.nlog >= LJ_CL_LOG_MAX) return -1;
        r->st.log_ms[r->st.nlog++] = (uint32_t)r->st.tc_ms;
        lj_cl_changed(r);
        return 0;
    default: return -1;
    }
}

int lj_cl_run_step(lj_cl_run *r, uint64_t now_us, const lj_cl_host *h) { return lj_cl_pass(r, now_us, h, 0); }

void lj_cl_run_unpin(lj_cl_run *r, const lj_cuelist *cl)
{
    if (r->cl == cl) { r->cl = NULL; r->st.state = LJ_CL_IDLE; r->st.next = 0; r->st.serial++; }
}
