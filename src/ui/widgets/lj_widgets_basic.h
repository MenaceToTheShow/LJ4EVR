/* lj_widgets_basic.h - the basic Delphi widget classes (design 7.2, drawing rules 7.3,
 * instantiation 7.6, keyboard 7.9): TLabel TPanel TEdit TMaskEdit TNumEdit TButton
 * TBitBtn TSpeedButton TCheckBox TRadioButton TRadioGroup TGroupBox TBevel TShape
 * TStaticText TImage TPaintBox TSplitter TODPanel2 TODPanel.
 *
 * Every widget is ONE calloc (lj_w_alloc, shared with the lists group) whose first
 * member is lj_widget; free() releases it. Constructors read the lj_ctl_desc (NULL =
 * code-built: zero geometry, class defaults) and append the widget to parent.
 * paint() paints the widget itself only; the window walks the children in DFM
 * order (= z-order) and clips nothing: a widget stays inside its own rect.
 * Mouse coordinates arrive relative to the widget; a pressed button expects the
 * window to keep sending MOUSE_MOVE/UP until the button goes up (Delphi capture). */
#ifndef LJ_UI_WIDGETS_WIDGETS_BASIC_H
#define LJ_UI_WIDGETS_WIDGETS_BASIC_H

#include <stddef.h>
#include <stdint.h>
#include "ui/lj_widget.h"
#include "ui/forms/lj_form.h"
#include "ui/widgets/lj_widgets_lists.h"      /* lj_w_alloc, lj_w_font_of, lj_w_text, lj_w_emit, lj_widget_ctor, LJ_VK_* */
#include "ui/widgets/lj_w_label.h"
#include "ui/widgets/lj_w_panel.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_maskedit.h"
#include "ui/widgets/lj_w_numedit.h"
#include "ui/widgets/lj_w_button.h"
#include "ui/widgets/lj_w_bitbtn.h"
#include "ui/widgets/lj_w_speedbutton.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_radiobutton.h"
#include "ui/widgets/lj_w_radiogroup.h"
#include "ui/widgets/lj_w_groupbox.h"
#include "ui/widgets/lj_w_bevel.h"
#include "ui/widgets/lj_w_shape.h"
#include "ui/widgets/lj_w_statictext.h"
#include "ui/widgets/lj_w_image.h"
#include "ui/widgets/lj_w_paintbox.h"
#include "ui/widgets/lj_w_splitter.h"
#include "ui/widgets/lj_w_odpanel2.h"
#include "ui/widgets/lj_w_odpanel.h"

#define LJ_VK_BACK   0x08
#define LJ_VK_DELETE 0x2E

#define LJ_W_TEXT_CAP 256                                   /* longest DFM caption is 80 bytes; MaxLength > 255 is clamped */

/* The captioned classes (label, panel, buttons, check/radio, group boxes, static
 * text, ODPanel) start with this; the bits serve the button-like ones. */
typedef struct { lj_widget w; char caption[LJ_W_TEXT_CAP]; unsigned pressed:1, hover:1, checked:1, down:1; int index; } lj_w_cap;

int lj_widgets_register_basic(lj_widget_ctor *table, int count);   /* fills the 20 slots of this group; -1 if count < LJ_K_COUNT */

/* Shared by the class files (lj_widgets_basic.c). */
void       *lj_w_cap_new(lj_widget *parent, const lj_ctl_desc *d, const lj_widget_vt *vt, size_t size, int tabstop);   /* lj_w_alloc + caption copied + ParentColor resolved */
uint32_t    lj_w_fg(const lj_widget *w);                                     /* Font.Color or clWindowText */
void        lj_w_caption_text(lj_surface *s, const lj_widget *w, int x, int y, const char *str);   /* caption in the fg colour with the accelerator underline; disabled = #808080 over white +1,+1 */
void        lj_w_focus(lj_widget *w);                                        /* lj_widget_set_focus + OnExit/OnEnter */
int         lj_w_plain_mouse(lj_widget *w, const lj_mouse_ev *m);            /* OnMouseDown/Up/Move/Wheel/DblClick, OnClick on up; 1 when a handler is bound */
int         lj_w_button_mouse(lj_w_cap *b, const lj_mouse_ev *m, int takes_focus);   /* left press/release with capture semantics; lj_w_click on release inside */
void        lj_w_click(lj_widget *w);                                        /* TControl.Click: the class action (toggle/check/latch) then OnClick */
int         lj_w_dialog_char(lj_widget *root, int ch);                       /* Alt+letter: clicks/focuses the first visible enabled widget with "&ch"; 1 when handled */
const char *lj_w_caption(const lj_widget *w);                                /* NULL for an uncaptioned class */
int         lj_w_set_caption(lj_widget *w, const char *s);                   /* runtime captions [F19]; -1 for an uncaptioned class */

#endif /* LJ_UI_WIDGETS_WIDGETS_BASIC_H */
