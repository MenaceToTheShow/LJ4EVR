/* lj_help_viewer.c - LJ_WIN_HELP, the bundled help viewer (lj_help_viewer.h; design 13 item 12, W4-HELP). Code-built:
 * LJ2's Help items opened WinHelp / the CHM, which LJ4EVR does not ship (D2), so there is no DFM. A topic list on the
 * left, the topic text on the right; the topics are the help/ text files compiled in (lj_help_topics_gen.c, tools/help_check.py). */
#include "windows/lj_help_viewer.h"

#include <string.h>
#include "core/lj_log.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_help_topics_gen.h"

/* PROPOSAL rig: no LJ2 window to measure (WinHelp was an OS window); sized so a 72-column topic line fits the memo */
static const lj_ctl_desc lj_hv_ctls[] = {
    { LJ_K_LISTBOX, "lbTopics", 0, 0, 170, 400, .align = LJ_AL_LEFT, .tab_order = 0, .font_inherited = 1,
      .events = { { "OnClick", "lbTopicsClick" } } },
    { LJ_K_MEMO, "mText", 170, 0, 470, 400, .align = LJ_AL_CLIENT, .readonly = LJ_YES, .scroll_bars = "ssVertical", .tab_order = 1,
      .font_inherited = 1 },
};
static const lj_form_desc lj_form_HELPVIEWER = {
    .name = "HelpViewerForm", .class_name = "THelpViewer", .caption = "LJ4EVR Help",   /* PROPOSAL: our caption, no LJ2 window */
    .client_width = 640, .client_height = 400, .color = LJ_CLR(0xC0C0C0) /* clBtnFace */, .border = LJ_BS_SIZEABLE,
    .icons = LJ_BI_GIVEN | LJ_BI_SYSMENU, .position = LJ_PO_SCREENCENTER, .text_height = 13, .font = LJ_FONT_SANS13,
    .font_name = "MS Sans Serif", .font_height = -11, .ctls = lj_hv_ctls, .n = LJ_COUNT_OF(lj_hv_ctls),
    .events = { { "OnClose", "FormClose" } },
};

#define LJ_HV_TOPICS 64
#define LJ_HV_LINES  512
static struct { lj_window *win; int topic; const char *titles[LJ_HV_TOPICS]; const char *lines[LJ_HV_LINES]; } hv;

static int ntopics(void) { return lj_help_ntopics < LJ_HV_TOPICS ? lj_help_ntopics : LJ_HV_TOPICS; }

int lj_help_viewer_find(const char *topic)
{
    int i;
    if (!topic || !topic[0]) return 0;
    for (i = 0; i < ntopics(); i++) if (strcmp(lj_help_topics[i].name, topic) == 0) return i;
    return -1;
}

int lj_help_viewer_current(void) { return hv.win ? hv.topic : -1; }

static void show_topic(int t)
{
    const lj_help_topic *ht = &lj_help_topics[t];
    int i, n = 0;
    hv.topic = t;
    hv.lines[n++] = ht->title;
    hv.lines[n++] = "";
    for (i = 0; i < ht->n && n < LJ_HV_LINES; i++) hv.lines[n++] = ht->lines[i];
    lj_w_memo_set_lines(lj_window_find(hv.win, "mText"), hv.lines, n);
    {
        lj_w_list *l = (lj_w_list *)lj_window_find(hv.win, "lbTopics");
        if (l) { l->index = t; lj_w_sbar_reveal(&l->sb, t, l->base.h / (l->item_h > 0 ? l->item_h : 13)); lj_widget_invalidate(&l->base); }
    }
}

static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; lj_help_viewer_close(LJ_WIN_HELP); }
static void on_topic(lj_widget *w, void *ud) { const lj_w_list *l = (const lj_w_list *)w; (void)ud; if (l->index >= 0 && l->index < ntopics()) show_topic(l->index); }
static const lj_binding lj_hv_bindings[] = { { "FormClose", on_close, NULL }, { "lbTopicsClick", on_topic, NULL }, { NULL, NULL, NULL } };

lj_window *lj_help_viewer_open(enum lj_win id)
{
    int i;
    if (id != LJ_WIN_HELP) return NULL;
    if (hv.win) return hv.win;
    hv.win = lj_window_create(&lj_form_HELPVIEWER, lj_hv_bindings);
    if (!hv.win) { lj_log("help: cannot create the viewer"); return NULL; }
    for (i = 0; i < ntopics(); i++) hv.titles[i] = lj_help_topics[i].title;
    lj_w_list_set_items(lj_window_find(hv.win, "lbTopics"), hv.titles, ntopics());
    show_topic(0);
    lj_win_show(hv.win, 0);
    return hv.win;
}

void lj_help_viewer_close(enum lj_win id)
{
    if (id != LJ_WIN_HELP || !hv.win) return;
    lj_win_hide(hv.win);
    lj_window_destroy(hv.win);
    hv.win = NULL;
}

lj_window *lj_help_viewer_get(enum lj_win id) { return id == LJ_WIN_HELP ? hv.win : NULL; }
void       lj_help_viewer_refresh(enum lj_win id) { (void)id; }   /* static text: nothing follows the model */
void       lj_help_viewer_bind_fns(void) { }                      /* LJ_ACT_HELP / SELECT_HELPFILE are bound in app/lj_app_w4.c */

int lj_help_viewer_topic(const char *topic)
{
    int t = lj_help_viewer_find(topic);
    if (t < 0) lj_log("help: no topic '%s', showing the index", topic);
    if (!lj_help_viewer_open(LJ_WIN_HELP)) return -1;
    show_topic(t < 0 ? 0 : t);
    return t < 0 ? -1 : 0;
}

void lj_help_viewer_click(lj_widget *w, void *ud) { (void)w; lj_help_viewer_topic((const char *)ud); }
