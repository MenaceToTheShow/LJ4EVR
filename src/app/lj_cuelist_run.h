/* lj_cuelist_run.h - the cue list runner (design 2.3 rule 4, 10 W3; 03 s6): a state machine over one lj_cuelist that
 * runs on the UI thread, stepped from the app's idle pass on every SDL_WaitEventTimeout(10) wake (LJ_CL_STEP_MS),
 * never in the engine. It does no I/O and no loading itself: every effect goes out through the lj_cl_host the caller
 * supplies (the app: lj_app_load_cue / lj_ctx_post / lj_fn_run; the replay harness: its pinned loads at synthetic
 * time), so the same code runs in lj4evr, in tests/app/test_cuelist_run.c and in the cuelist_run replay. SDL-free;
 * linked into lj4evr_tests by target_sources like lj_app_seam.c.
 *
 * Line semantics (contract for the cuelist-core track, model/lj_cuelist.h header comment):
 *  - a line = condition then action. Condition: its Wait (elapsed since the previous line executed, PC clock, date
 *    change, MTC) on an AUTO line, or Go ! on a MANUAL line, whose Wait is ignored (LJ_CL_MANUAL_IGNORES_WAITS).
 *    A line with no Wait on an auto line fires in the same step as the line before it, so consecutive auto lines with
 *    no command latch several cues on one Go (03 s6.1, non-transparent cue first).
 *  - action: the non-wait command (goto_line, execute_file, hotkey_command, flash_sequence, load_background_cue,
 *    link_cuelist), then the cue latch (host->load_cue) or the slot trigs (one LJ_CMD_TRIG per slot of trig_mask with
 *    b = 0xFF: "each of the sequences will be trigged exactly once ... in the programmed loop direction").
 *  - load_error lines (CD / DA / MP, design 8.5) are skipped as if absent; the list runs the rest.
 *  - the list ends (state IDLE) after the last line or on STOP; Goto Line# beyond the last line ends it (RN v2.6 b7).
 *  - Run to line executes only the cue commands up to and including the line [RN v2.5 b3]; Tracking on (lj_cuelist.
 *    tracking) applies the same rule to Jump / Goto Mark / Run from line (PROPOSAL rig 03 s6: LJ2's tracking mode is
 *    not described beyond its caption); Automatic Cue update on Run loads the last non-transparent cue before the run
 *    line, then the transparent ones between it and the line.
 *  - Execute File runs only when Main.Ini [CUELIST2] AllowExecute=1 (ours, default 0: a show file must not start
 *    programs on another machine unasked); otherwise the line is logged and skipped. The host decides.
 * Bodies: lj_cuelist_run.c (cuelist-core track). */
#ifndef LJ_APP_CUELIST_RUN_H
#define LJ_APP_CUELIST_RUN_H

#include <stdint.h>
#include "engine/lj_engine.h"
#include "model/lj_cuelist.h"

typedef struct {
    void *ud;
    int  (*load_cue)(void *ud, uint32_t number);                 /* latch a cue (the app: lj_app_load_cue(number, 0)); -1 = load error, the list goes on */
    int  (*post)(void *ud, const lj_cmd *cmd);                   /* LJ_CMD_TRIG / LJ_CMD_FLASH ... (the app: lj_ctx_post) */
    int  (*load_bg)(void *ud, uint32_t number);                  /* Load background cue */
    int  (*flash)(void *ud, uint32_t seq, int on);               /* Flash Sequence: the host pins the sequence (FLASH b 1); the runner releases it (on = 0) after param_ms */
    int  (*link)(void *ud, uint32_t cuelist, const lj_cuelist **out);   /* Link Cuelist: the host loads it and hands back the pinned list; the run continues at its line 1 */
    void (*fn)(void *ud, int fn);                                /* Hotkey Command: lj_fn_run(fn, 1), the "on" half only [RN v2.5 b5] */
    int  (*exec)(void *ud, const char *path, const char *args);  /* Execute File (see the header comment); -1 = refused / failed, the list goes on */
    void (*clear_cue)(void *ud);                                 /* Cuelist Stop and Clear: clears the current cue */
    int  (*timecode)(void *ud, int src, uint32_t *ms);           /* enum lj_cl_tc_src: 0 with the time, -1 when that source has none (CD / DA / MP always -1 in W3) */
    int  (*clock)(void *ud, uint32_t *ms_of_day, uint32_t *day); /* local wall clock for Wait PC-Clock / Wait Date-Change (day = any monotonic day number) */
} lj_cl_host;

enum lj_cl_mtc_timeout { LJ_CL_MTC_STOP = 0 /* M_MTCStop */, LJ_CL_MTC_RESTART /* M_MTCRestart */, LJ_CL_MTC_IGNORE /* MTC_NoTimeout */ };
typedef struct {
    lj_cl_status st;                     /* what the window paints (ctx->cl points here) */
    const lj_cuelist *cl;                /* the list running, pinned: "the list is locked while active" [03 s6.1] */
    uint64_t run_t0_us, line_t0_us;      /* Top / Run time; when the previous line executed (the Wait Elapsed base) */
    uint32_t day0;                       /* the day number when the date-change wait began */
    uint32_t flash_seq; uint64_t flash_off_us;   /* a running Flash Sequence line and its release time */
    uint8_t  ignore_first_wait;          /* M_IgnoreFirstWait "Ignore first Wait Elapsed on Run" */
    uint8_t  auto_cue_on_run;            /* M_AutoCue "Automatic Cue update on Run" */
    uint8_t  mtc_timeout;                /* enum lj_cl_mtc_timeout */
    uint8_t  first_pending;              /* internal: the first Wait Elapsed after Run is still ahead */
    uint8_t  in_exec;                    /* internal (ME-108): a line's host callback is running; a nested Go returns -1, Stop / unpin act at once */
    uint8_t  mtc_seen;                   /* internal (ME-116): the previous pass had a time code; present -> absent at a Wait MTC applies mtc_timeout */
} lj_cl_run;

void lj_cl_run_init(lj_cl_run *r);                                                    /* IDLE, tc source PC elapsed */
/* Transport (enum lj_cl_op; n = 1-based line or mark). cl = the list the op applies to (the window's current list);
 * ops on an IDLE runner pin it. 0 / -1 (bad line, empty list, op invalid in this state). */
int  lj_cl_run_op(lj_cl_run *r, const lj_cuelist *cl, int op, uint32_t n, uint64_t now_us, const lj_cl_host *h);
/* One UI wake: every line whose condition holds executes, in order, until one waits; flash releases and the shown time
 * code are updated. Returns the number of lines executed (0 = still waiting or idle). */
int  lj_cl_run_step(lj_cl_run *r, uint64_t now_us, const lj_cl_host *h);
void lj_cl_run_unpin(lj_cl_run *r, const lj_cuelist *cl);                             /* the app is about to free cl: the runner stops if it runs it (2.3 rule 6 for the UI side) */

#endif /* LJ_APP_CUELIST_RUN_H */
