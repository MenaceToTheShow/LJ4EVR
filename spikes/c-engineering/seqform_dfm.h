/* seqform_dfm.h - TSequenceForm transcribed from
 * D:/lj4evr/reference/installer/exe_resources/dfm_text/TSEQUENCEFORM.dfm.txt
 * This is the shape a DFM-to-C generator emits: one row per control, design
 * coordinates at 96 dpi, captions verbatim (including Martin's '&' accelerators).
 * Nothing here is Martin artwork: geometry, captions and class kinds are facts. */
#ifndef SEQFORM_DFM_H
#define SEQFORM_DFM_H

#define SEQFORM_CAPTION       "Sequence"
#define SEQFORM_CLIENT_WIDTH  466
#define SEQFORM_CLIENT_HEIGHT 163
#define SEQFORM_TEXT_HEIGHT   13   /* MS Sans Serif -11 at PixelsPerInch 96 */

/* Panel1 is at Left=0 Top=0 in the DFM, which covers the three labels and edits
 * (07-visual-reference.md section 5 and its Unverified list). The runtime
 * position is not in any source; the spike pushes the panel below the edit row
 * so every required control is visible. One constant, replaced by the rig. */
#define SEQFORM_PANEL1_TOP_SPIKE 40

enum widget_kind { W_LABEL, W_EDIT, W_PANEL, W_BEVEL, W_SPEEDBTN, W_PANEL_LOWERED2,
                   W_CHECKBOX, W_UDBUTTON, W_MENUBAR };

struct widget {
    enum widget_kind kind;
    int parent;            /* index of the containing panel, -1 = form */
    int x, y, w, h;        /* DFM Left/Top/Width/Height, relative to parent */
    const char *caption;   /* DFM Caption or Text; NULL when none */
    const char *name;      /* DFM object name, for tests and hit-test ids */
};

/* index 0..5: form-level controls; 6: Panel1; 7..: children of Panel1 */
static const struct widget SEQFORM[] = {
    { W_LABEL,  -1,   7,   2,  26, 13, "Seq#",              "Label1" },
    { W_LABEL,  -1,  47,   2,  80, 13, "Sequence Name",     "Label2" },
    { W_LABEL,  -1, 215,   2,  76, 13, "Sequence Type",     "Label3" },
    { W_EDIT,   -1,   7,  15,  33, 21, "",                  "ESeqNo"   }, /* clSilver, ReadOnly */
    { W_EDIT,   -1,  47,  15, 160, 21, "",                  "ESeqName" },
    { W_EDIT,   -1, 215,  15, 145, 21, "",                  "ESeqType" },
    { W_PANEL,  -1,   0,   0, 354, 106, NULL,               "Panel1"   }, /* BevelOuter = bvNone */
    { W_BEVEL,   6,   1,   2, 350, 33, NULL,                "Bevel3"   },
    { W_SPEEDBTN,6,   5,   6,  25, 25, NULL,                "sb3"  },
    { W_SPEEDBTN,6,  31,   6,  25, 25, NULL,                "sb4"  },
    { W_BEVEL,   6,  59,   6,  73, 25, NULL,                "Elapsed" },
    { W_SPEEDBTN,6, 133,   6,  25, 25, NULL,                "sb1"  },
    { W_SPEEDBTN,6, 159,   6,  25, 25, NULL,                "sb2"  },
    { W_PANEL_LOWERED2, 6, 186, 6, 54, 25, NULL,            "Panel2" },
    { W_SPEEDBTN,6, 243,   6,  51, 25, NULL,                "sb7"  },
    { W_SPEEDBTN,6, 298,   6,  25, 25, NULL,                "sb5"  },
    { W_SPEEDBTN,6, 324,   6,  25, 25, NULL,                "sb6"  },
    { W_LABEL,   6,   4,  41,  83, 13, "Scene Time (sec)",  "Label5" },
    { W_LABEL,   6,   4,  65,  78, 13, "Fade Time (Sec)",   "Label6" },
    { W_EDIT,    6,  95,  37,  40, 21, "88:88",             "ES" },
    { W_EDIT,    6,  95,  61,  40, 21, "88:88",             "EF" },
    { W_UDBUTTON,6, 137,  37,  20, 21, NULL,                "UdScene" },
    { W_UDBUTTON,6, 136,  61,  20, 21, NULL,                "UdFade" },
    { W_LABEL,   6, 163,  41,  64, 13, "XXXX.X BPM",        "lBPM" },
    { W_CHECKBOX,6, 239,  39, 113, 17, "Global &Scene Time","cbGlobalScene" },
    { W_CHECKBOX,6, 239,  58, 113, 17, "Global &Fade Time", "cbGlobalFade" },
    { W_CHECKBOX,6,   4,  86, 195, 17, "S&ynchronize Scene and Fade Times", "cbSync" },
    { W_SPEEDBTN,6, 225,  78,  25, 25, NULL,                "sb8"  },
    { W_SPEEDBTN,6, 250,  78,  25, 25, NULL,                "sb9"  },
    { W_SPEEDBTN,6, 275,  78,  25, 25, NULL,                "sb10" },
    { W_SPEEDBTN,6, 300,  78,  25, 25, NULL,                "sb11" },
    { W_SPEEDBTN,6, 326,  78,  25, 25, NULL,                "seqBlind" },
};
#define SEQFORM_COUNT ((int)(sizeof SEQFORM / sizeof SEQFORM[0]))

/* MainMenu1 top-level items, DFM order */
static const char *const SEQFORM_MENU[] = { "Disable Fade", "&Preferences", "Snapshot", "&Help" };
#define SEQFORM_MENU_COUNT 4

#endif
