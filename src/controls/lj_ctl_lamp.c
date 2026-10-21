/* lj_ctl_lamp.c - lamp on / lamp off / reset buttons with the confirm box, and the note box (note 15/01 s9, s10). */
#include "controls/lj_ctl_lamp.h"

#include <stdlib.h>
#include <string.h>
#include "gfx/lj_draw.h"
#include "gfx/lj_font.h"
#include "ui/widgets/lj_widgets_basic.h"

static const char *confirm_text(const char *caption)
{
    if (!caption) return NULL;
    if (strstr(caption, "Off") || strstr(caption, "off")) return LJ_CTL_LAMP_OFF_TEXT;
    if (strstr(caption, "Reset") || strstr(caption, "reset")) return LJ_CTL_RESET_TEXT;
    return NULL;
}

static void lamp_event(lj_widget *w, int ev, void *ud)
{
    lj_ctl_elem *e = (lj_ctl_elem *)ud;
    const char *text;
    (void)w;
    if (ev != LJ_WEV_CLICK || !e->btn) return;
    text = confirm_text(e->btn->caption);
    if (text && !lj_ctl_confirm(LJ_CTL_CONFIRM_CAPTION, text)) return;
    lj_ctl_write(e, e->btn->value);                                  /* lamp_on_ms / reset_ms serve Auto Strike Lamps (W2), not this window: the scene holds the value */
}

static int build_buttons(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    int i, n = row->nbuttons, bw;
    if (n <= 0) return 0;
    if (x + w > c->desc.client_width) w = c->desc.client_width - x - 1;   /* lampcontrol.gif: two 41-px buttons in a 127-px client */
    bw = (w - (n - 1) * LJ_CTL_GAP) / n;
    for (i = 0; i < n; i++) {
        lj_widget *b = lj_ctl_button_new(c, parent, row, &row->buttons[i], LJ_CE_BUTTON, row->buttons[i].caption, x + i * (bw + LJ_CTL_GAP), y, bw, LJ_CTL_ROW_H, 0);
        if (b) b->on_event = lamp_event;
        if (b && row->kind == LJ_W_LAMP_BTNS && row->buttons[i].caption && strstr(row->buttons[i].caption, "Lamp O")) {   /* lampcontrol.gif: lit / dark bulb glyphs, the caption becomes the hint (integrator; our art, D2) */
            lj_ctl_desc *d = (lj_ctl_desc *)b->desc;
            d->glyph = strstr(row->buttons[i].caption, "Off") ? "lampoff" : "lamp"; d->hint = row->buttons[i].caption; d->caption = NULL; d->show_hint = LJ_YES;
            lj_w_set_caption(b, "");
        }
    }
    return LJ_CTL_ROW_H;
}

/* the note box: white sunken panel over the rest of the client, the caption word-wrapped into labels (lampcontrol.gif y 56-93) */
static int build_note(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int y)
{
    int cw = c->desc.client_width, h = c->desc.client_height - y, line = 0, maxw = cw - 8;
    lj_ctl_elem *e = lj_ctl_elem_add(c, parent, NULL, NULL, LJ_CE_NONE);
    lj_ctl_desc *d;
    const lj_font *f;
    const char *s = row->caption ? row->caption : "";
    if (!e || h <= 4) return 0;
    d = lj_ctl_desc_new(e, LJ_K_PANEL, NULL, 0, y, cw, h);
    d->color = LJ_CLR(0xFFFFFF); d->bevel_outer = LJ_BV_LOWERED;
    e->w = lj_w_panel_create(parent, d);
    f = lj_w_font_of(e->w);
    while (*s && (line + 1) * f->height + 1 <= h) {                 /* three 13-px lines in the 40-px box (lampcontrol.gif y 56-93) */                 /* greedy wrap at spaces; a line is a label that points into the caption */
        const char *end = s, *brk = NULL;
        lj_ctl_elem *le;
        char *copy;
        size_t len;
        const char *nl = strchr(s, '\n');
        while (*end && !(nl && end > nl)) {
            const char *sp = strchr(end, ' ');
            size_t n = sp && !(nl && sp > nl) ? (size_t)(sp - s) : nl ? (size_t)(nl - s) : strlen(s);
            char probe[128];
            if (n >= sizeof probe) n = sizeof probe - 1;
            memcpy(probe, s, n); probe[n] = 0;
            if (lj_text_width(f, probe) > maxw && brk) break;
            brk = s + n;
            if (!sp || (nl && sp > nl)) break;
            end = sp + 1;
        }
        if (!brk) brk = nl ? nl : s + strlen(s);
        len = (size_t)(brk - s);
        le = lj_ctl_elem_add(c, parent, NULL, NULL, LJ_CE_NONE);
        if (!le) break;
        copy = (char *)calloc(1, sizeof(lj_ctl_desc) + len + 1);   /* desc + the line's text in one block, freed with the desc */
        if (!copy) lj_fatal("lj_ctl: out of memory");
        memcpy(copy + sizeof(lj_ctl_desc), s, len);
        le->d = (lj_ctl_desc *)copy;
        le->d->kind = LJ_K_LABEL; le->d->caption = copy + sizeof(lj_ctl_desc); le->d->autosize = LJ_NO; le->d->parent_color = LJ_YES;
        le->d->left = 3; le->d->top = 1 + line * f->height; le->d->width = maxw; le->d->height = f->height;
        le->w = lj_w_label_create(e->w, le->d);
        line++;
        s = brk; while (*s == ' ' || *s == '\n') s++;
    }
    return h;
}

int lj_ctl_lamp_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w)
{
    switch (row->kind) {
    case LJ_W_LAMP_BTNS: return build_buttons(c, parent, row, x, y, w);
    case LJ_W_NOTE:      return build_note(c, parent, row, y);
    default: return 0;
    }
}
