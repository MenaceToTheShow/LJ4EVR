/* test_remote_form.c - `lj4evr_tests test_remote_form` (W5-REM-4): TRS232SettingsForm headless.
 *  - a UI script opens remote_setup, checks the harvested captions and the DFM's bOk Enabled = False, ticks TCP, picks
 *    57600, OKs: ctx->remote and Hardware.Ini [REMOTE] carry the settings, the listener is open on 127.0.0.1
 *    (the interface default), and the window reads them back on the next open;
 *  - the windows-layer state query drives the set-style commands from ctx->view (132 1 x: the smoke timer).
 * The render is written to test_remote_form_tmp/remote_setup.bmp for review (no golden: the POSIX port captions and the
 * interface list are the machine's). */
#include <stdio.h>
#include <string.h>
#include "app/lj_app_io.h"
#include "app/lj_uiscript.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "gfx/lj_bmp.h"
#include "io/lj_fn.h"
#include "io/lj_remote.h"
#include "io/lj_socket.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "windows/lj_remote_form.h"
#include "windows/lj_windows.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define TMP "test_remote_form_tmp/"

static int run_text(const char *text)
{
    uint64_t t0 = lj_clock_us();
    FILE *f = fopen(TMP "remote.lj4s", "wb");
    int r;
    if (!f) return -2;
    fputs(text, f); fclose(f);
    if ((r = lj_uiscript_load(TMP "remote.lj4s")) != 0) return r;
    while ((r = lj_uiscript_step()) == 1 && lj_clock_us() - t0 < 2000000u) {}
    return r;
}

static int hits;
static void rec(enum lj_fn fn, int edge, uint32_t prog, void *ud) { (void)fn; (void)edge; (void)prog; (void)ud; hits++; }

int lj_test_remote_form(int argc, char **argv);
int lj_test_remote_form(int argc, char **argv)
{
    static const uint8_t timer_on[] = { 132, 1, 1 };
    lj_ctx *ctx = lj_ctx_get();
    lj_ini *hw = lj_ini_new();
    lj_remote_stats st;
    lj_window *w;
    int r;
    (void)argc; (void)argv;
    lj_fs_mkdir(TMP);
    ctx->hardware_ini = hw; ctx->engine = NULL;
    lj_str_copy(ctx->userdir, sizeof ctx->userdir, TMP);
    lj_app_io_set_platform(NULL);
    lj_app_io_start();                                              /* ctx->remote, LJ_ACT_IO_APPLY */
    CHECK(ctx->remote && !ctx->remote->tcp_on && !ctx->remote->serial_on);
    ctx->remote->tcp_port = 47232;                                  /* away from the PROPOSAL default on a busy runner */
    r = run_text("open remote_setup\n"
                 "assert_caption TRS232SettingsForm \"RS232 Settings\"\n"
                 "assert_text TRS232SettingsForm.rgPort \"COM Port\"\n"
                 "assert_text TRS232SettingsForm.rgBaudRate \"Baud Rate\"\n"
                 "assert_text TRS232SettingsForm.bOk \"&Ok\"\n"
                 "assert_text TRS232SettingsForm.bCancel \"&Cancel\"\n"
                 "assert_enabled TRS232SettingsForm.bOk 0\n"
                 "click TRS232SettingsForm.cbTcp\n"
                 "assert_enabled TRS232SettingsForm.bOk 1\n");
    CHECK(r == 0);
    w = lj_win_get(LJ_WIN_REMOTE_SETUP);
    CHECK(w != NULL);
    if (w) {
        lj_w_radiogroup_set_index(lj_window_find(w, "rgBaudRate"), 6);
        lj_window_paint(w);
        CHECK(lj_bmp_write(lj_window_surface(w), TMP "remote_setup.bmp") == 0);
    }
    r = run_text("click TRS232SettingsForm.bOk\n");
    CHECK(r == 0);
    CHECK(lj_win_get(LJ_WIN_REMOTE_SETUP) == NULL);
    CHECK(ctx->remote->tcp_on && ctx->remote->tcp_ip == LJ_IP_LOOPBACK && ctx->remote->tcp_port == 47232 && ctx->remote->baud == 57600 && !ctx->remote->serial_on);
    CHECK(lj_ini_geti(hw, "REMOTE", "TcpEnabled", 0) == 1 && lj_ini_geti(hw, "REMOTE", "TcpPort", 0) == 47232 && lj_ini_geti(hw, "REMOTE", "Baud", 0) == 57600);
    CHECK(strcmp(lj_ini_gets(hw, "REMOTE", "TcpIp", ""), "127.0.0.1") == 0);
    lj_remote_get_stats(&st);
    CHECK(st.tcp_open && st.tcp_port == 47232);                    /* LJ_ACT_IO_APPLY reopened the transports */
    r = run_text("open remote_setup\nassert_enabled TRS232SettingsForm.bOk 0\nassert_text TRS232SettingsForm.eTcpPort \"47232\"\nclick TRS232SettingsForm.bCancel\n");
    CHECK(r == 0 && lj_win_get(LJ_WIN_REMOTE_SETUP) == NULL);
    lj_remote_close();

    /* the windows-layer query: 132 1 1 turns the smoke timer on only when the view says it is off */
    lj_remote_form_bind_fns();
    lj_fn_bind(LJ_FN_TOGGLE_SMOKE_TIMER, rec, NULL);
    memset(&ctx->view, 0, sizeof ctx->view);
    ctx->view.tick_no = 1; ctx->view.smoke_timer = 0;
    lj_remote_inject(timer_on, sizeof timer_on, 1000);
    CHECK(hits == 1);
    ctx->view.smoke_timer = 1;
    lj_remote_inject(timer_on, sizeof timer_on, 2000);
    CHECK(hits == 1);
    r = run_text("remote 132 1 0\n");                               /* the script verb: the same feed and exec */
    CHECK(r == 0 && hits == 2);
    lj_remote_set_query(NULL);
    lj_app_io_stop();
    printf("test_remote_form: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
