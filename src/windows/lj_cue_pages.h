/* lj_cue_pages.h - cue pages in the List of Cues ([03 s5.8], F28): the page tabs (TC1) and
 * page list (lvPages) of TAdvancedCueForm, its Pages menu / page popup / page buttons, the
 * Cue Page hotkeys, and the two page dialogs: TGetPageInfoForm (caption "Add New Cue Page",
 * "Insert New Cue Page", "Edit Page Name"; 254x77, MaxLength 20) and TReorderCuePagesForm
 * ("Re-order Cue Pages", drag a row to move a page; Ok applies, Cancel drops). The pages are
 * ctx->cue_pages (model/lj_cuepage.h); every edit is written to Cues\CuePageInfo.Dat at once
 * ("Error Saving Cue Page File" on failure, exe 0x164114). The List of Cues binds the
 * lj_cue_pages_on_* handlers (lj_cue_form.c). */
#ifndef LJ_WINDOWS_CUE_PAGES_H
#define LJ_WINDOWS_CUE_PAGES_H

#include <stdint.h>
#include "model/lj_cuepage.h"
#include "ui/lj_window.h"

enum lj_page_name_mode { LJ_PAGE_NAME_ADD = 0, LJ_PAGE_NAME_INSERT, LJ_PAGE_NAME_EDIT };

lj_cuepages *lj_cue_pages_model(void);            /* ctx->cue_pages; when the app has not set it, a static one loaded from the library on first use; never NULL */
int          lj_cue_pages_store(void);            /* CuePageInfo.Dat; 0 / -1 (+ the exe's error box) */

/* TGetPageInfoForm */
lj_window *lj_cue_page_name_open(void);           /* LJ_WIN_CUE_PAGE_NAME: Add New Cue Page */
lj_window *lj_cue_page_name_open_mode(enum lj_page_name_mode mode);
void       lj_cue_page_name_close(void);          /* = Cancel */
lj_window *lj_cue_page_name_get(void);

/* TReorderCuePagesForm */
lj_window *lj_cue_pages_reorder_open(void);
void       lj_cue_pages_reorder_close(void);      /* = Cancel */
lj_window *lj_cue_pages_reorder_get(void);
int        lj_cue_pages_reorder_move(int from, int to);   /* the drop on lvPages (working copy); 0 / -1 */

/* List of Cues glue */
void     lj_cue_pages_fill(lj_window *lc);        /* TC1 tabs = page names, lvPages rows "0007\tname" of the current page */
int      lj_cue_pages_command(const char *item);  /* a Pages menu / page popup item by DFM name (M_AddnewPage, PopClearPage, ...); 0 handled, -1 unknown */
int      lj_cue_pages_step_page(int delta);       /* Previous/Next Cue Page (Circular Page Selection); the new page or -1 */
uint32_t lj_cue_pages_step_cue(int delta, int latch);   /* Cue Page Select (latch 0) / Latch (latch 1) Previous/Next Cue; the cue or 0 */
void lj_cue_pages_on_tab(lj_widget *w, void *ud);        /* TC1Change */
void lj_cue_pages_on_menu(lj_widget *w, void *ud);       /* PageMenuClick, PagesPopClick */
void lj_cue_pages_on_button(lj_widget *w, void *ud);     /* sbPrev / sbNext / sbPrevPage / sbNextPage / sbPageGo MouseDown (right button: select without latching) */
void lj_cue_pages_on_dblclick(lj_widget *w, void *ud);   /* lvPagesDblClick: load */
void lj_cue_pages_on_drop(lj_widget *w, void *ud);       /* lvPagesDragDrop: a cue from the global list, inserted at the drop row */
void lj_cue_pages_on_prefs(lj_widget *w, void *ud);      /* PrefsClick of the List of Cues (forms.ini [ACUESELECT] keys) */
int  lj_cue_pages_add_global(uint32_t cue, int insert);  /* GlobalPop "Add Cues to Page" / "Insert Cues on Page" */
void lj_cue_pages_bind_fns(void);                        /* Previous/Next Cue Page, Cue Page Latch/Select Previous/Next Cue, Load Selected Cue (Cue Page) */

#endif /* LJ_WINDOWS_CUE_PAGES_H */
