/* lj_w_bitbtn.c - TBitBtn: a TButton whose Kind supplies the caption when the DFM gives
 * none (bkOK -> "OK" ...) and the default/cancel role (lj_w_button_is_default; the
 * window reads desc->cancel and desc->modal_result). desc->glyph names our art for a
 * Glyph.Data; a Kind with no Glyph draws the kind's art (lj_w_bitbtn_kind_glyph, ME-136). */
#include "ui/widgets/lj_w_bitbtn.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "core/lj_str.h"

static const lj_widget_vt vt = { lj_w_button_paint, lj_w_button_mouse_ev, lj_w_button_key, NULL, NULL };

static const struct { const char *kind, *caption, *glyph; } lj_bk[] = {   /* glyph: our CC0 art stem for the kind (ME-136; the stock glyphs are Borland's, never shipped) */
    { "bkOK", "OK", "ok" }, { "bkCancel", "Cancel", "cancel" }, { "bkHelp", "Help", "help" }, { "bkClose", "Close", "close" }, { "bkYes", "Yes", "yes" },
    { "bkNo", "No", "no" }, { "bkRetry", "Retry", "retry" }, { "bkIgnore", "Ignore", "ignore" }, { "bkAbort", "Abort", "abort" }, { "bkAll", "All", "all" },
};

const char *lj_w_bitbtn_kind_glyph(const lj_widget *w)
{
    int i;
    if (!w || w->kind != LJ_K_BITBTN || !w->desc || w->desc->glyph || !w->desc->kind_str) return NULL;
    for (i = 0; i < LJ_COUNT_OF(lj_bk); i++) if (strcmp(lj_bk[i].kind, w->desc->kind_str) == 0) return lj_bk[i].glyph;
    return NULL;
}

lj_widget *lj_w_bitbtn_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cap *b = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 1);
    int i;
    if (d && !d->caption && d->kind_str)
        for (i = 0; i < LJ_COUNT_OF(lj_bk); i++)
            if (strcmp(lj_bk[i].kind, d->kind_str) == 0) { lj_str_copy(b->caption, sizeof b->caption, lj_bk[i].caption); break; }
    return &b->w;
}
