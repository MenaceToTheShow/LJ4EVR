/* lj_prefs.c - see lj_prefs.h. */
#include "app/lj_prefs.h"

#include <string.h>
#include "core/lj_log.h"
#include "engine/lj_engine_rules.h"
#include "io/lj_artnet.h"
#include "io/lj_net.h"
#include "ui/lj_formsini.h"

int lj_prefs_tick_hz(lj_ini *main_ini)
{
    int hz = lj_ini_geti(main_ini, "ENGINE", "TickHz", LJ_TICK_HZ_DEFAULT);
    if (hz < LJ_TICK_HZ_MIN) hz = LJ_TICK_HZ_MIN;
    if (hz > LJ_TICK_HZ_MAX) hz = LJ_TICK_HZ_MAX;
    return hz;
}

int lj_prefs_sacn(lj_ini *hardware_ini, const char *path, lj_sacn_cfg *cfg)
{
    const char *rej;
    if (lj_sacn_cfg_load(hardware_ini, cfg) != 0) {              /* no CID yet: defaults, generated once and persisted (w1-contracts) */
        char host[64];
        lj_net_hostname(host, sizeof host);
        lj_sacn_cfg_default(cfg, host);
        lj_sacn_cfg_save(hardware_ini, cfg);
        if (lj_ini_save(hardware_ini, path) != 0) lj_log("prefs: cannot save %s", path);
    }
    lj_artnet_cfg_load(hardware_ini, cfg);                        /* W5: [ARTNET] per-link protocol (missing = sACN everywhere) */
    rej = lj_sacn_cfg_validate(cfg);
    if (rej) { lj_log("Hardware.Ini [SACN]: %s", rej); return -1; }
    return 0;
}

static lj_ini *lj_prefs_ini_for(lj_ini *forms_ini, lj_ini *main_ini, const lj_window *win, char *sec, size_t cap)
{
    int where = lj_formsini_section(lj_window_desc(win), sec, cap);
    if (where < 0) return NULL;
    return where == LJ_FORMSINI_MAIN ? main_ini : forms_ini;
}

int lj_prefs_window_pos(lj_ini *forms_ini, lj_ini *main_ini, const lj_window *win, int *x, int *y)
{
    char sec[64];
    lj_ini *ini = lj_prefs_ini_for(forms_ini, main_ini, win, sec, sizeof sec);
    return ini ? lj_formsini_restore(ini, sec, x, y, NULL, NULL) : -1;
}

void lj_prefs_window_save(lj_ini *forms_ini, lj_ini *main_ini, const lj_window *win, int x, int y)
{
    char sec[64];
    const lj_surface *s = lj_window_surface((lj_window *)win);
    lj_ini *ini = lj_prefs_ini_for(forms_ini, main_ini, win, sec, sizeof sec);
    if (ini) lj_formsini_save(ini, sec, lj_formsini_sizeable(lj_window_desc(win)), x, y, s->w, s->h);
}
