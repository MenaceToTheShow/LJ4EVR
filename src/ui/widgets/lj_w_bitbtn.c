/* lj_w_bitbtn.c - TBitBtn: a TButton whose Kind supplies the caption when the DFM gives
 * none (bkOK -> "OK" ...) and the default/cancel role (lj_w_button_is_default; the
 * window reads desc->cancel and desc->modal_result). Glyphs are Martin art [D2]: the
 * glyph name in desc->glyph waits for the art pass; the caption is centred meanwhile. */
#include "ui/widgets/lj_w_bitbtn.h"
#include "ui/widgets/lj_widgets_basic.h"

#include <string.h>
#include "core/lj_str.h"

static const lj_widget_vt vt = { lj_w_button_paint, lj_w_button_mouse_ev, lj_w_button_key, NULL, NULL };

static const struct { const char *kind, *caption; } lj_bk[] = {
    { "bkOK", "OK" }, { "bkCancel", "Cancel" }, { "bkHelp", "Help" }, { "bkClose", "Close" }, { "bkYes", "Yes" },
    { "bkNo", "No" }, { "bkRetry", "Retry" }, { "bkIgnore", "Ignore" }, { "bkAbort", "Abort" }, { "bkAll", "All" },
};

lj_widget *lj_w_bitbtn_create(lj_widget *parent, const lj_ctl_desc *d)
{
    lj_w_cap *b = lj_w_cap_new(parent, d, &vt, sizeof(lj_w_cap), 1);
    int i;
    if (d && !d->caption && d->kind_str)
        for (i = 0; i < LJ_COUNT_OF(lj_bk); i++)
            if (strcmp(lj_bk[i].kind, d->kind_str) == 0) { lj_str_copy(b->caption, sizeof b->caption, lj_bk[i].caption); break; }
    return &b->w;
}
