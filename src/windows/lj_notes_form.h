/* lj_notes_form.h - the Notes window (TNoteForm "Notes", [01 s4]: a plain memo, stay-on-top,
 * View > Notes, hotkey Toggle Notes, Startup Options "Show Notes on Startup"). The text is the
 * library's Notes.txt (model/lj_notes.h, PROPOSAL file name), loaded at open and saved on close
 * when changed. Also the small memo editor the Backup dialog's notes box shares: TMemo / TRichEdit
 * are read-only widgets (ui owner, recorded), so lj_memo_edit wraps the widget's key handler with
 * typing at the end of the text (characters, Enter, Backspace). */
#ifndef LJ_WINDOWS_NOTES_FORM_H
#define LJ_WINDOWS_NOTES_FORM_H

#include <stddef.h>
#include "model/lj_notes.h"
#include "windows/lj_windows.h"

lj_window *lj_notes_form_open(void);            /* LJ_WIN_NOTES */
void       lj_notes_form_close(void);           /* saves Notes.txt when the text changed */
lj_window *lj_notes_form_window(void);
void       lj_notes_form_refresh(void);         /* the pending close */
const char *lj_notes_form_text(void);           /* the memo's text ("" when closed) */
int        lj_notes_form_set_text(const char *text);   /* replaces the text (marks it changed); -1 when closed or too long */

/* ---- the shared memo editor ---- */
enum { LJ_MEMO_EDIT_LINES = 1024 };             /* ponytail: lines past this are kept in the text but not shown; grow when a note needs more */
typedef struct { lj_widget *memo; char text[LJ_NOTES_CAP]; char split[LJ_NOTES_CAP]; const char *line[LJ_MEMO_EDIT_LINES]; int changed; } lj_memo_edit;
void lj_memo_edit_attach(lj_memo_edit *e, lj_widget *memo);   /* the memo's key handler types into e->text (caret at the end: ponytail, a caret model when someone edits mid-text) */
void lj_memo_edit_detach(lj_memo_edit *e);
int  lj_memo_edit_set(lj_memo_edit *e, const char *text);    /* -1 when longer than LJ_NOTES_CAP - 1 (nothing changed) */

#endif /* LJ_WINDOWS_NOTES_FORM_H */
