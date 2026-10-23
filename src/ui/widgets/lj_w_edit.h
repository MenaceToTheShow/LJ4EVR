/* lj_w_edit.h - TEdit, and the struct TNumEdit/TMaskEdit extend; see lj_widgets_basic.h.
 * Text is CP1252 bytes; caret and anchor are byte offsets, selection = [min, max). */
#ifndef LJ_UI_WIDGETS_W_EDIT_H
#define LJ_UI_WIDGETS_W_EDIT_H

#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"

enum { LJ_W_EDIT_PLAIN = 0, LJ_W_EDIT_NUM, LJ_W_EDIT_MASK };

typedef struct {
    lj_widget w;
    char text[256];                 /* MaxLength is clamped to 255 */
    int  len, caret, anchor, max_length, variant;
    int  min, max, value;           /* TNumEdit */
    char mclass[256];               /* TMaskEdit: per position 'd' digit, 'a' letter, 'n' alnum, 'c' any, 0 literal */
    char blank;                     /* TMaskEdit blank character */
    unsigned readonly:1, hide_sel:1, password:1, pressed:1;
} lj_w_edit;

lj_widget  *lj_w_edit_create(lj_widget *parent, const lj_ctl_desc *d);
const char *lj_w_edit_text(const lj_widget *w);                      /* NULL for a non-edit */
int         lj_w_edit_int(const lj_widget *w, int lo, int hi, int dflt);   /* ME-173: the text as a decimal in lo..hi (spaces around allowed); empty, not a number or out of range = dflt */
int         lj_w_edit_set_text(lj_widget *w, const char *s);          /* clamps to MaxLength, caret home, fires OnChange; -1 for a non-edit */
void        lj_w_edit_select(lj_widget *w, int start, int len);       /* SelStart/SelLength; len < 0 = to the end */
int         lj_w_edit_insert(lj_w_edit *e, int ch);                   /* one validated CP1252 byte at the caret (replaces the selection); 1 when the text changed */
extern const lj_widget_vt lj_w_edit_vt;

#endif /* LJ_UI_WIDGETS_W_EDIT_H */
