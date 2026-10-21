/* lj_windows.h - the behaviour-heavy windows of W1 (design 2.9, 7.4, 10 W1):
 * one open/close/refresh entry per window, captions per F19 / 13 s5, the red
 * warning boxes and the close prompt. Each window is a generated form
 * (ui/forms/lj_forms_gen.c) plus a behaviour file binding every OnXxx its DFM
 * declares (ME-16): lj_seq_form.c lj_cue_form.c lj_save_dialogs.c lj_lists.c
 * lj_fixture_config.c lj_master_intensity.c lj_hardware_setup.c
 * lj_dmx_monitor.c lj_prefs.c lj_warnings.c (the design's file names carry the
 * lj_ prefix the layering test requires). Positions persist through
 * ui/lj_formsini.h with LJ2's section names (7.10).
 *
 * Seam 3 (windows <-> engine view): a window never touches the engine's state;
 * it copies lj_engine_status / lj_frame under lj_lock in its refresh and
 * paints from the copy (the Cue grid "1:2", the Sequence progress bars, the
 * DMX Output Monitor cells), and acts through lj_ctx_post(). */
#ifndef LJ_WINDOWS_WINDOWS_H
#define LJ_WINDOWS_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include "desktop/lj_desktop.h"
#include "ui/lj_window.h"

enum lj_win {
    LJ_WIN_SEQ_FORM = 0,     /* TSequenceForm  "SEQUENCE [0000 No Name]"  [SEQ] */
    LJ_WIN_CUE_FORM,         /* TCurrentCue    "CUE [0001 name]"          [CUE] */
    LJ_WIN_SAVE_SEQ,         /* TSaveSeqForm   */
    LJ_WIN_SAVE_CUE,         /* TSaveCueForm   [SAVECUEFORM] AutoClose SaveOnEnter SaveMacro */
    LJ_WIN_LIST_SEQ,         /* TSeqListForm   */
    LJ_WIN_LIST_CUE,         /* TCueListForm (global list) [ACUESELECT] */
    LJ_WIN_FIXTURE_CONFIG,   /* TFixtureConfigForm 790x590 */
    LJ_WIN_MASTER_INTENSITY, /* TMasterIntensityForm 664x297 [GLOBALINTENSITY] */
    LJ_WIN_HARDWARE_SETUP,   /* THardwareForm: the sACN page where Martin's cards were [08 s7] */
    LJ_WIN_DMX_MONITOR,      /* TDMXOUTFORM 356x264 + the code-built grid [F49, ME-28] */
    LJ_WIN_STARTUP_OPTIONS,  /* TStartupForm [01 s6.2] */
    LJ_WIN_WARNINGS,         /* TSTATUSWARNINGFORM / TFREEZEWARNFORM: the red boxes */
    LJ_WIN_CLOSE_PROMPT,     /* TCONFIRMFORM "Close Lightjockey ?" */
    LJ_WIN_PLEASE_WAIT,      /* the startup status window "Please Wait.." (LORD_FIDELITY-20) */
    LJ_WIN_COUNT
};

lj_window *lj_win_open(enum lj_win id);                    /* creates (bindings, forms.ini position, platform open through the app hook) or raises; NULL + log */
void       lj_win_close(enum lj_win id);                   /* saves the position, destroys */
void       lj_win_toggle(enum lj_win id);
lj_window *lj_win_get(enum lj_win id);                     /* NULL when closed */
void       lj_win_refresh(enum lj_win id);                 /* repaint from the engine view / model (seam 3) */
void       lj_win_refresh_all(void);                       /* the 100 ms hook */
void       lj_win_close_all(void);                         /* shutdown and "Close All Windows" */
lj_window *lj_win_find(const char *form);                  /* by DFM class name ("TSequenceForm") or window id name ("seq_form"): the UI script driver's resolver */
const char *lj_win_class(enum lj_win id);                  /* "TSequenceForm" ... */
const char *lj_win_id_name(enum lj_win id);                /* "seq_form" ... the docs/ui-script.md window names */

/* The app supplies how a created lj_window reaches the screen (platform/ is not includable here). */
typedef struct { void *ud; int (*open)(void *ud, lj_window *w, unsigned flags); void (*close)(void *ud, lj_window *w); void (*show)(void *ud, lj_window *w, int show);
                 void (*move)(void *ud, lj_window *w, int x, int y); int (*pos)(void *ud, const lj_window *w, int *x, int *y); } lj_win_host;   /* move/pos appended by windows-c (LORD_FIDELITY-12: the warning boxes centre on the main window); NULL = not available */
void lj_win_set_host(const lj_win_host *host);             /* once at startup; NULL = headless (tests, lj4evr_render); also registers the windows' lj_fn rows */

/* captions (F19, 13 s5): 4-digit numbers, "No Name" for an unsaved record, verbatim literals */
int lj_win_caption_seq(char *dst, size_t cap, uint32_t number, const char *name);            /* "SEQUENCE [0000 No Name]" */
int lj_win_caption_cue(char *dst, size_t cap, uint32_t number, const char *name, int edit);  /* "CUE [0009 name]", "EDIT CUE [0009 name]" (prefix condition UNVERIFIED) */
int lj_win_caption_cuelist(char *dst, size_t cap, uint32_t number, const char *name, int tracking);   /* "Cuelist [0001 name]" + " Tracking on" */
int lj_win_caption_main(char *dst, size_t cap, const char *library);                         /* "Martin LightJockey (User lib: Default)" */

/* warnings [03 s9, 13 s2.2]: the red boxes, stay-on-top, one per state */
enum lj_warning { LJ_WARN_BLACKOUT = 0 /* "BLACKOUT ACTIVE" */, LJ_WARN_FROZEN /* "Frozen !" */, LJ_WARN_FADE_OUT /* "FADE-OUT ACTIVE" */, LJ_WARN_FADE_IN /* "FADE-IN ACTIVE" */, LJ_WARN_OFFLINE, LJ_WARN_COUNT };
void lj_win_warning(enum lj_warning w, int show);          /* driven from lj_desktop_refresh by the engine view; Main.Ini [PREFS] Show*Warning gates */

/* close prompt: "Close Lightjockey ?" (TCONFIRMFORM, Yes/No); result through the callback on the UI thread */
void lj_win_close_prompt(void (*on_result)(int yes, void *ud), void *ud);

/* load-error / confirm boxes the W1 windows raise (caption, text verbatim from 13 s2.2; Ok only) */
void lj_win_message(const char *caption, const char *text);

/* windows-c additions (appended, lj_windows.c): the behaviour files reach the screen through the host the app installed */
const lj_win_host *lj_win_host_get(void);                   /* NULL = headless */
int  lj_win_show(lj_window *w, unsigned flags);             /* host->open, 0 when headless */
void lj_win_hide(lj_window *w);                             /* host->close, no-op when headless */

#endif /* LJ_WINDOWS_WINDOWS_H */
