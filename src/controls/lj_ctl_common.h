/* lj_ctl_common.h - what the per-kind files of controls/ share (bodies in
 * lj_ctl_common.c): the control-window record, the per-widget binding record
 * (one lj_ctl_elem per widget bound to a row), the geometry constants measured
 * from the 1:1 CHM GIFs (note 07 s14, design 7.7), the generic row renderers
 * and the write path every handler funnels through (seam 2 of
 * docs/w1-contracts.md). The public API is controls/lj_ctl.h; this header adds
 * the hooks the app wires (platform host, confirm box) and the builder the
 * tests drive with their own tables.
 *
 * Row kinds and who renders them (lj_ctl_row_build dispatches by row->kind):
 *   common:    LJ_W_FADEBTN LJ_W_DRAGBAR LJ_W_PALETTE LJ_W_EFFECT_BTN LJ_W_SCROLL
 *   intensity: LJ_W_SHUTTER_TOGGLE LJ_W_SPECIAL_GROUP
 *   levels:    LJ_W_LEVEL_FADER (a column: caption, fader, O/S/F)
 *   lamp:      LJ_W_LAMP_BTNS (value buttons; "Off"/"Reset" captions confirm) LJ_W_NOTE
 *   extended:  LJ_W_RADIOGROUP_FADER (a column: group box, radios, speed fader)
 *   controls-b: LJ_W_ROTATION LJ_W_INDEXBAR LJ_W_MODEPAIR LJ_W_SHAKE LJ_W_WHEELSCROLL (0 px here, logged once) */
#ifndef LJ_CONTROLS_CTL_COMMON_H
#define LJ_CONTROLS_CTL_COMMON_H

#include <stdint.h>
#include "controls/lj_ctl.h"
#include "ui/forms/lj_form.h"
#include "ui/lj_window.h"
#include "windows/lj_windows.h"

enum {
    /* ME-67: intensitycontrol.gif (480x98) is a 0.8 reduction (its O/S/F reads 39 wide where off.gif, 1:1, is 48x24); every
     * number below is the GIF measurement x1.25 snapped to the 1:1 anchors off.gif 48x24 and the 26-px pitch (3 rows = the
     * 78-px client of the 600x122 window). "measured from intensitycontrol.gif, reduced x1.25, rig item" throughout. */
    LJ_CTL_PITCH      = 26,    /* row pitch: O/S/F 24 + 2 (GIF rows y 33-52 / 53-71 / 75-92 = 20 x1.25) */
    LJ_CTL_ROW_H      = 24,    /* off.gif / snap.gif / fade.gif 48x24 (1:1 crops), design 7.7 */
    LJ_CTL_OSF_W      = 48,    /* off.gif 48x24 (the GIF's fade column x 2-40 = 39 x1.25 = 48.75) */
    LJ_CTL_X          = 51,    /* content column: GIF x 43 minus the 2-px frame = 41, x1.25 */
    LJ_CTL_BAR_W      = 169,   /* GIF x 43-177 = 135, x1.25 */
    LJ_CTL_SPEC_X     = 226,   /* specials block: GIF label x 183-244, buttons from 247, minus the frame, x1.25 */
    LJ_CTL_SPEC_LBL_W = 78,
    LJ_CTL_SPEC_BTN_X = 306,
    LJ_CTL_GAP        = 4,     /* GIF gap between the special buttons 3, x1.25 */
    LJ_CTL_SHUTTER_OPEN = 20,  /* PROPOSAL rig F47: the one value "Shutter Open" emits inside the Martin 20-49 range */
    LJ_CTL_ELEMS      = 96,
    LJ_CTL_MENU_MAX   = 8
};

/* what a bound widget is, so one refresh and one write path serve every kind */
enum lj_ctl_role { LJ_CE_NONE = 0 /* label, box: owns a desc only */, LJ_CE_OSF, LJ_CE_DRAGBAR, LJ_CE_SHUTTER, LJ_CE_BUTTON, LJ_CE_FADER, LJ_CE_RADIO, LJ_CE_PALETTE, LJ_CE_SCROLL };

typedef struct {
    lj_widget *w;
    const lj_ctl_row *row;           /* the table row; row->ch is the profile offset */
    const lj_ctl_button *btn;        /* LJ_CE_BUTTON / LJ_CE_RADIO / LJ_CE_PALETTE: what a click emits (lo..hi = when it reads as selected) */
    lj_ctl_desc *d;                  /* a desc this layer owns for the widget (colours, caption), freed on close; NULL for plain code-built widgets */
    uint8_t role, dflt_state;        /* dflt_state: the governing O/S/F row's dflt_state (PROPOSAL: an Off channel goes to it when a control is touched) */
    uint8_t lo, hi, reverse;         /* value range of a bar / fader (row lo..hi, or the shutter's closed/open pair) */
    unsigned special:1;              /* part of the Intensity specials block (Preferences shows/hides it) */
    char text[12];                   /* a generated caption ("CH8") the widget's desc points at */
    char name[32];                   /* the name the --script driver addresses the widget by: row<ch+1>_<bar|fade|shutter|btn<i>|radio<i>|pal<i>|scroll> (docs/ui-script.md) */
} lj_ctl_elem;

typedef struct lj_ctl_win {
    enum lj_ctl_kind kind;
    lj_window *win;                  /* NULL = closed */
    const lj_ctl_window *table;
    const lj_profile *profile;
    lj_form_desc desc;               /* hand-built: caption, client size, border, menu; class name chosen so lj_formsini_section() yields INTENSITY / LEVELS / ... */
    lj_menu_desc menu; lj_menu_item_desc items[LJ_CTL_MENU_MAX]; char hname[LJ_CTL_MENU_MAX][40];
    lj_binding bind[LJ_CTL_MENU_MAX + 2];
    lj_ctl_elem e[LJ_CTL_ELEMS]; int n;
    uint8_t dflt[LJ_PROFILE_CH];     /* dflt_state of the last O/S/F row built per offset (0 = none yet -> LJ_SNAP) */
    unsigned specials_shown:1;
    unsigned foreign:1;              /* integrator: a controls-b/c window (lj_ctl_movement_* / lj_ctl_c_*) registered here so get/close/refresh/toggle see it */
    int cw_full, cw_plain;           /* Intensity: client width with / without the specials block */
} lj_ctl_win;

/* hooks the app wires (none = headless: windows are built, confirms say yes and log) */
void lj_ctl_set_host(const lj_win_host *host);                                         /* the same open/close/show hook windows/ takes; open flags = 2u (LJ_WF_STAY_ON_TOP of platform/lj_platform.h, not includable here) */
void lj_ctl_set_confirm(int (*fn)(const char *caption, const char *text, void *ud), void *ud);   /* Yes/No box: "Confirm" / "Lamp Power Off\n(Warning : ...)" [13 s2.2]; returns 1 = yes */
int  lj_ctl_confirm(const char *caption, const char *text);
int  lj_ctl_menu_run(enum lj_ctl_kind kind, const char *item);   /* W4: the UserPalette / Fades / Help bar items of a control window [04 s1.6]; 0 handled, -1 not one of them */

/* the builder (lj_ctl_open resolves table/profile from the master; tests pass their own) */
lj_window  *lj_ctl_open_table(enum lj_ctl_kind kind, const lj_ctl_window *table, const lj_profile *p);
lj_ctl_win *lj_ctl_of(const lj_window *win);                                           /* the record owning win, NULL */
lj_ctl_win *lj_ctl_rec(enum lj_ctl_kind kind);                                         /* the record (open or not) */

/* registration and widgets the kind files build with */
lj_ctl_elem *lj_ctl_elem_add(lj_ctl_win *c, lj_widget *w, const lj_ctl_row *row, const lj_ctl_button *btn, int role);   /* NULL when the window is full (logged) */
lj_widget   *lj_ctl_elem_find(const lj_window *win, const char *name);   /* the elem widget of a control window by its row<ch>_<role> name; NULL */
lj_ctl_desc *lj_ctl_desc_new(lj_ctl_elem *e, enum lj_ctl_row_kind kind, const char *caption, int x, int y, int w, int h);  /* an owned desc for a basic widget */
lj_widget   *lj_ctl_button_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, const lj_ctl_button *btn, int role,
                               const char *caption, int x, int y, int w, int h, int black);    /* TSpeedButton; black = white text on black (the specials, note 15/01 s4) */
lj_widget   *lj_ctl_dragbar_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, const char *caption, int x, int y, int w);   /* bar over row->lo..hi (reverse honoured), LJ_CTL_ROW_H tall */
lj_widget   *lj_ctl_label_new(lj_ctl_win *c, lj_widget *parent, const char *caption, int x, int y, int w, int h, int center);
lj_widget   *lj_ctl_osf_new(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w, int h);        /* O/S/F bound to row->ch; states mask = row->states_allowed, else SWITCH class -> OFF|SNAP (ME-25), else all */
unsigned     lj_ctl_states(const lj_ctl_win *c, const lj_ctl_row *row);
uint8_t      lj_ctl_dflt_state(const lj_ctl_win *c, uint8_t offset);

/* the write path: every handler ends here */
void lj_ctl_on_event(lj_widget *w, int ev, void *ud);   /* the generic dispatcher for LJ_CE_OSF/DRAGBAR/FADER/SCROLL/BUTTON/PALETTE widgets (ud = the elem) */
void lj_ctl_write(lj_ctl_elem *e, uint8_t value);       /* selection-wide value write at e->row->ch (Off -> dflt_state promotion), then repaint of every open control */
void lj_ctl_repaint_all(void);                          /* every open control from the current scene (the 100 ms hook without the master check) */
void lj_ctl_win_repaint(lj_ctl_win *c);

#endif /* LJ_CONTROLS_CTL_COMMON_H */
