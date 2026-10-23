/* lj_cuelist.h - CueLists\NNNNNNNN.Cl2 and AutoSave.Cl2 (design 5.2: "LJ4 cl 1",
 * num name tracking, "mark k=line", then "line n adv=auto|manual cmd=<snake
 * case of 03 s6.3> param= pstr= ref= cue= trig= comment=..."). W1 shipped the
 * file and the line edits; W3 adds the command enum, the verbatim captions, the
 * load-error rule of design 8.5, the parameter encodings and the status record
 * the UI-thread runner (app/lj_cuelist_run.h) publishes for the window.
 *
 * Line model [03 s6.1-6.3]: a line is a CONDITION (its wait command, or Go on a
 * manual line: "Command is ignored in Manual Mode" for every Wait) followed by an
 * ACTION (its non-wait command, then the cue latch `cue` or the slot trigs
 * `trig_mask`, never both). Parameters per command (`param_ms` unless named):
 *   wait_elapsed      ms since the previous line executed (Min 0-99, Sec 0-59, frames 0-99 = 1/100 s)
 *   wait_pc_clock     ms since local midnight (H 0-23, M, S)
 *   wait_date_change  -  (the local date rolls over)
 *   wait_mtc          ms of the H:M:S:F time code (lj_cl_hmsf_ms with the fps); pstr = "24" | "25" | "30" (the panel's fps button)
 *   autoseek_mtc      -
 *   goto_line         the 1-based target line; beyond the last line = ignored [RN v2.6 b7]
 *   execute_file      pstr = the program path; comment = its parameters ("Comment/Exe Parameter" column)
 *   hotkey_command    enum lj_fn (io/lj_actions.h, append-only so the number is stable); flash functions run only their "on" half [RN v2.5 b5]
 *   flash_sequence    duration ms (<= LJ_CL_FLASH_MAX_MS); ref = sequence number
 *   load_background_cue  ref = bg cue number
 *   link_cuelist      ref = cue list number (unsaved changes are lost at the link)
 *   set_current_cue, set_cue_trig   panel actions that fill `cue` / `trig_mask`; never a stored cmd (validate rejects them)
 *   CD / DA rows parameters kept as loaded (load_error = 1, design 8.5, ME-11)
 *   mp_load_file      pstr = the WAV path; a path that does not exist loads with load_error = 1 (red, skipped)
 *   mp_seek, mp_wait  ms of the H:M:S:hh time code (fps 100: frames 0-99, TClEditPanels UpDown28 / UpDown32 Max 99)
 * Bodies: lj_cuelist.c (cuelist-core track). */
#ifndef LJ_MODEL_CUELIST_H
#define LJ_MODEL_CUELIST_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_model.h"

int         lj_cuelist_load(const char *path, lj_cuelist *out, char *err, size_t cap);   /* lines allocated (cap >= n); 0 / -1. W3: reads ref=, sets load_error on CD/DA/MP lines (kept, not rejected) */
int         lj_cuelist_save(const char *path, const lj_cuelist *in);                      /* W3: writes ref=; a load_error line is written with the words it was read with */
const char *lj_cuelist_validate(const lj_cuelist *in);
lj_cuelist *lj_cuelist_new_rec(void);
void        lj_cuelist_free(lj_cuelist *cl);                                              /* frees lines and the record; NULL is a no-op */
int         lj_cuelist_insert(lj_cuelist *cl, uint32_t before, const lj_cl_line *line);   /* grows lines; renumbers .line 1..n; 0 / -1 */
int         lj_cuelist_delete(lj_cuelist *cl, uint32_t index);

extern const char *const lj_cl_cmd_words[];   /* cmd= words in 03 s6.3 order, index = enum lj_cl_cmd ("none" first); NULL-terminated */

/* ---- W3: the command enum, in lj_cl_cmd_words order (the file words are its names in snake case) ---- */
enum lj_cl_cmd {
    LJ_CL_NONE = 0,
    LJ_CL_WAIT_CD, LJ_CL_CD_PLAY_FROM_TRACK, LJ_CL_CD_PLAY_FROM_TIME, LJ_CL_CD_STOP,
    LJ_CL_WAIT_DA, LJ_CL_PLAY_DA_FILE, LJ_CL_CUE_DA_FILE, LJ_CL_DA_STOP, LJ_CL_SEEK_DA, LJ_CL_DA_PLAY, LJ_CL_DA_TIMECODE_FACTOR,
    LJ_CL_MP_LOAD_FILE, LJ_CL_MP_PLAY, LJ_CL_MP_SEEK, LJ_CL_MP_STOP, LJ_CL_MP_PAUSE, LJ_CL_MP_WAIT, LJ_CL_MP_CLOSE_CLIP, LJ_CL_MP_START, LJ_CL_MP_CLOSE,
    LJ_CL_WAIT_MTC, LJ_CL_AUTOSEEK_MTC, LJ_CL_WAIT_ELAPSED, LJ_CL_WAIT_PC_CLOCK, LJ_CL_WAIT_DATE_CHANGE,
    LJ_CL_GOTO_LINE, LJ_CL_EXECUTE_FILE, LJ_CL_SET_CURRENT_CUE, LJ_CL_SET_CUE_TRIG, LJ_CL_HOTKEY_COMMAND,
    LJ_CL_FLASH_SEQUENCE, LJ_CL_LOAD_BACKGROUND_CUE, LJ_CL_LINK_CUELIST,
    LJ_CL_CMD_COUNT
};
/* Verbatim captions, index = enum lj_cl_cmd (D13, never corrected):
 * lj_cl_cmd_caption = the command panel / ListPop / TAction captions of TCuelist2Form (13-dfm-captions.tsv; "Goto Line#",
 *   "Wait PC-Clock", "MP Load File" ...); the three drag-only commands use the exe pool words "Flash Seq", "Load BGCue",
 *   "Link Cuelist" (0x17A090 0x17A030 0x17A044); none = "".
 * lj_cl_cmd_grid = the Command column text the grid shows (exe pool 0x179F08-0x17A194: "Wait for CD at", "CD Play Track",
 *   "Wait PC-Clock at", "Wait PC date-change", "Goto Line", "Execute" ...); a command with no pool row shows its caption
 *   (PROPOSAL rig 03 s6: MP Load File, Auto Seek MTC, Hotkey Command); none = "<>" (0x179F14, cuelist1.gif lines 1 / 4). */
extern const char *const lj_cl_cmd_caption[LJ_CL_CMD_COUNT];
extern const char *const lj_cl_cmd_grid[LJ_CL_CMD_COUNT];
extern const char *const lj_cl_adv_caption[2];   /* the Adv column: "Auto", "Man" (exe 0x179EF8, 0x179F08) */

enum lj_cl_family { LJ_CLF_NONE = 0, LJ_CLF_CD, LJ_CLF_DA, LJ_CLF_MP, LJ_CLF_MTC, LJ_CLF_PC, LJ_CLF_OTHER, LJ_CLF_DRAG };
int lj_cl_cmd_family(int cmd);    /* enum lj_cl_family; -1 out of range */
int lj_cl_cmd_inert(int cmd);     /* 1 for the CD / DA families: greyed with hint LJ_CL_NOT_AVAILABLE, loaded red, skipped (design 8.5, ME-11); MP is live from W5 (io/lj_media.h) */
int lj_cl_cmd_is_wait(int cmd);   /* 1 for every Wait (ignored on a manual line) */
#define LJ_CL_NOT_AVAILABLE "Not available in LJ4EVR"   /* design 8.5 hint; ours, not in the exe pool (LORD_FIDELITY-14 tag) */
#define LJ_CL_FLASH_MAX_MS 99900                         /* "duration up to 99.9 s" [RN v2.6 b4] */
#define LJ_CL_LOG_MAX 256                                /* Log time code entries kept per run (PROPOSAL; LJ2's list is unbounded) */

/* time code fields <-> param_ms. fps: 100 for PC time (frames = 1/100 s), 75 CD, 24 / 25 / 30 MTC, 100 DA / MP.
 * A frame packs to ceil(f * 1000 / fps) ms so lj_cl_ms_hmsf gives the same frame back at every fps. */
uint32_t lj_cl_hmsf_ms(int h, int m, int s, int f, int fps);
void     lj_cl_ms_hmsf(uint32_t ms, int fps, int *h, int *m, int *s, int *f);
int      lj_cl_tc_text(char *dst, size_t cap, int32_t ms, int fps);   /* "HH:MM:SS:FF"; ms < 0 = "--:--:--:--" (lCurrentTime's DFM text); 0 / -1 */

/* ---- W3: what the UI-thread runner publishes (app/lj_cuelist_run.h fills it, the Cuelist window paints from it) ---- */
enum lj_cl_state { LJ_CL_IDLE = 0 /* passive edit: list editable */, LJ_CL_RUNNING /* active: waiting on an auto line's condition */, LJ_CL_WAIT_GO /* a manual line waits for Go ! */ };
enum lj_cl_tc_src { LJ_TC_PC_ELAPSED = 0 /* sbPCElapsed */, LJ_TC_PC_CLOCK /* sbPCClock */, LJ_TC_MIDI /* sbMIDITime */,
                    LJ_TC_CD /* sbCDTime, greyed */, LJ_TC_DA /* sbDATime, greyed */, LJ_TC_MP /* sbMPTime, the MediaPlayer clip clock (W5) */, LJ_TC_COUNT };
enum lj_cl_line_state { LJ_CLL_PLAIN = 0, LJ_CLL_DONE /* last executed: gray */, LJ_CLL_NEXT /* waiting: blue */, LJ_CLL_ERROR /* load_error: red */ };   /* [RN v2.6 b7], design 8.5 */
typedef struct {
    uint8_t  state;              /* enum lj_cl_state */
    uint32_t number;             /* the list running (0 = the unsaved working list) */
    uint32_t next, last;         /* 1-based: the line waiting (blue) and the last executed (gray); 0 = none */
    uint32_t elapsed_ms;         /* lTotalElapsed "Elapsed 0000:00:00" since Top / Run */
    uint8_t  tc_src;             /* enum lj_cl_tc_src the display shows */
    int32_t  tc_ms;              /* lCurrentTime, -1 = no time code from that source */
    uint16_t nlog; uint32_t log_ms[LJ_CL_LOG_MAX];   /* Log time code stamps (TLogTimeCodeForm Add / Insert / Apply) */
    uint32_t serial;             /* increments on every change; the window repaints when it moved */
} lj_cl_status;
int lj_cl_line_state(const lj_cl_status *st, const lj_cuelist *cl, uint32_t line);   /* enum lj_cl_line_state of a 1-based line */

/* Runner operations: the window, the hotkeys (LJ_FN_CUELIST_*) and the script reach the app's runner through
 * lj_desktop_run_action(LJ_ACT_CUELIST_OP, LJ_CL_OP_ARG(op, n)), n = 1-based line or mark 1..4 (0 when unused). */
enum lj_cl_op { LJ_CL_OP_TOP = 0 /* sbTop */, LJ_CL_OP_RUN /* sbRun: from line n */, LJ_CL_OP_GO /* sbGo: a waiting manual line; idle = Run from top */,
                LJ_CL_OP_BACK /* sbBack: one line up, not on line 1 */, LJ_CL_OP_STOP /* sbStop */, LJ_CL_OP_STOP_CLEAR /* Cuelist Stop and Clear: also clears the current cue */,
                LJ_CL_OP_JUMP /* Pop_JumpToThis: while waiting for Go */, LJ_CL_OP_RUN_TO /* M_RunToLine: cue commands only, up to and including n [RN v2.5 b3] */,
                LJ_CL_OP_MARK_GOTO /* sbMark1..4 click */, LJ_CL_OP_MARK_RUN /* sbMark right-click */, LJ_CL_OP_LOG /* sbLogAudio / [CTRL]: stamp the shown time code */,
                LJ_CL_OP_TC_SRC /* W5 media (appended): n = enum lj_cl_tc_src the display and Log time code follow (sb*Time) */,
                LJ_CL_OP_COUNT };
#define LJ_CL_OP_ARG(op, n) ((int)(op) | ((int)(n) << 8))
#define LJ_CL_OP_OF(arg)    ((arg) & 0xFF)
#define LJ_CL_OP_N(arg)     ((uint32_t)(arg) >> 8)

#endif /* LJ_MODEL_CUELIST_H */
