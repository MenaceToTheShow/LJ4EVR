/* lj_statics2_form.h - the Statics 2 window (TStatics2Form "Statics" 510x356 design, [14 sB.3], [03 s8], design 3.4
 * rank 4, ME-30). Page buttons in tbPages (one per show->pages entry, "Page %d" when unnamed), then per visible slot a
 * number, the slot button (the entry name; green and pressed while the engine view reports the slot active: the
 * Statics "LED"), the mode button (the replace scope word; left click PopReplaceMode, right click PopActivationMode)
 * and the level cell (" Level 100%", drag / wheel 0..100). Right click on a slot opens popStaticEntry verbatim from
 * the DFM, Martin's duplicated "Replace Page" / "Replace Slot" items included (D13, acceptance 4); its TAction items
 * are bound here. Release Page / Release All in pBottom.
 * Engine seam (docs/w2-contracts.md seam 1): activation posts LJ_CMD_STATIC_ACTIVATE a = lj_static_global_index(page,
 * slot), b = level_pct, c = the slot's enum lj_replace, st = &show->pages[p].slot[s] (pinned; the pages array never
 * moves, ME-91), and for a LINK slot seq[0] = its sequence from show->sequences, pinned in the same critical section
 * (loaded through LJ_ACT_PIN_SEQ when on disk only; the app re-pins it on a re-save or delete, ME-88); release posts
 * LJ_CMD_STATIC_DEACTIVATE; Clear Slot runs LJ_ACT_STATIC_CLEAR (the app hands the engine the release first).
 * Record / Link refuse an active slot (the engine reads its scene). The LEDs and the toolbar Statics LED follow
 * ctx->view (static_slot_of[] + static_active[], statics_any). */
#ifndef LJ_WINDOWS_STATICS2_FORM_H
#define LJ_WINDOWS_STATICS2_FORM_H

#include "engine/lj_engine.h"
#include "ui/lj_window.h"

#define LJ_S2_ROW_H 16   /* slot row pitch: PROPOSAL rig 14 sB.8 item 12 (runtime-built in LJ2; staticcontrol.gif, reduced, reads ~16 per entry) */

lj_window *lj_statics2_form_open(void);
void       lj_statics2_form_close(void);
lj_window *lj_statics2_form_get(void);
void       lj_statics2_form_refresh(void);                 /* the 100 ms hook: LEDs from ctx->view, the toolbar Statics LED */
void       lj_statics2_form_bind_fns(void);                /* Statics control, Static 01..20 (current page, PROPOSAL rig 14 sB.8 item 9), Release statics */
int        lj_statics2_page(void);                         /* the current page, 1-based */
int        lj_statics2_set_page(int page);                 /* 0 / -1 out of range */
int        lj_statics2_visible(void);                      /* slot rows shown: page n, else ctx->static_visible, else 20; 1..64 (LJ_STATICS2_SLOTS_MAX), 20 the default */
int        lj_statics2_press(int slot, int down);          /* the slot button on the current page (1-based): Toggle flips on press, Flash follows the button; 0 posted, -1 empty / out of range */
int        lj_statics2_slot_active(int slot);              /* from ctx->view, current page */
int        lj_statics2_set_level(int slot, int pct);       /* 0..100 into the slot under lj_lock; an active slot is re-posted with c = LJ_REP_ADD (no scope releases); [STATICS] LevelAutoEnable / LevelAutoDisable */
void       lj_statics2_release(int page);                  /* 0 = Release All, else Release Page */
void       lj_statics2_popup(int slot, int x, int y);      /* popStaticEntry for that slot at surface x,y (screen when a host is set) */
int        lj_statics2_popup_slot(void);                   /* the slot the last popup acts on, 1-based; 0 none */
lj_widget *lj_statics2_slot_button(int slot);              /* NULL when closed or not shown */
int        lj_statics2_last_cmd(lj_cmd *out);              /* the last command posted (tests); 0 = none */

/* ---- ME-91: the Statics Preferences settings, Main.Ini [STATICS2] with the exe's nine keys (13-exe-strings 0x214F2C..):
 * Width Height (the window), LvlVisible ModeVisible (View Levels / View Modes), SlotsVisible (Visible Slots 1..64, ME-30),
 * TabWidth TabHeight (0 = Auto) Multiline (the page buttons), PRWidth (the mode + level column, 202 in the DFM).
 * View Slot# has no key among the nine: it holds for the session (PROPOSAL rig 14 sB.8 item 12). */
typedef struct { int width, height, lvl_visible, mode_visible, slots_visible, tab_width, tab_height, multiline, pr_width, slot_no_visible; } lj_statics2_view;
void lj_statics2_view_get(lj_statics2_view *v);            /* from ctx->main_ini (defaults: the DFM's 510x356, both columns, 20 slots, Auto, multiline, 202) */
void lj_statics2_view_set(const lj_statics2_view *v);      /* into ctx->main_ini (all nine keys), ctx->static_visible and every page's n; an open window is rebuilt */
void lj_statics2_save(void);                               /* Statics.dat with the pages, slots and group names */

#endif /* LJ_WINDOWS_STATICS2_FORM_H */
