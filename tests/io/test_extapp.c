/* test_extapp.c - the extapp track (W5-EXT-1..4; io/lj_extapp.h, docs/extapp.md). Three subcommands:
 *  test_extapp      the 05 s10.2 grammar rows (TRIG!1 then TRIG!2 send TRIG twice, #X7F = 0x7F, #256 and 33 characters
 *                   refused), hostile Hardware.Ini / definition files, the engine's frame copy (topmost runner wins, only
 *                   built-in profile 90), and a loopback TCP client: a scene change delivers the bytes, an unchanged
 *                   string sends nothing, a dead peer is counted, the show keeps running and the link comes back.
 *  test_extapp_seq  the `ext` word of the scene record: round trip, a v1.0.0 file (no ext) loads unchanged, bad ext lines
 *                   refuse the file.
 *  test_extapp_ui   both entries headless: Fixture toolbar button 7 (lj_ctl_open Extended) on a profile-90 master builds
 *                   TSpecialSettings_ExternalApp (Set / Get Command write and read the scene), and Define Application
 *                   Link (LJ_ACT_EXTAPP_SETUP) opens TExternalAppSetupForm whose OK stores the link in [EXTAPP];
 *                   goldens CTL_EXTERNAL_EXTENDED and W5X_EXTAPP_SETUP (LJ_GOLDEN_UPDATE=1 rewrites them). */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/lj_app_io.h"
#include "controls/lj_ctl.h"
#include "controls/lj_ctl_extapp.h"
#include "core/lj_clock.h"
#include "core/lj_fsinfo.h"
#include "core/lj_str.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "desktop/lj_desktop.h"
#include "desktop/lj_selection.h"
#include "engine/lj_engine.h"
#include "engine/lj_runner.h"
#include "gfx/lj_bmp.h"
#include "io/lj_extapp.h"
#include "io/lj_net.h"
#include "io/lj_socket.h"
#include "model/lj_scene.h"
#include "model/lj_sequence.h"
#include "model/lj_show.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_extapp_form.h"
#include "windows/lj_windows.h"

#ifndef LJ_SOURCE_DIR
#define LJ_SOURCE_DIR "."
#endif

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define TMP "test_extapp_tmp/"

static int enc(const char *cmd, const char *want, size_t wn)       /* 1 when cmd encodes to exactly want[0..wn) */
{
    uint8_t out[LJ_EXTAPP_MAX_BYTES];
    size_t n = 99;
    char err[64];
    return lj_extapp_encode(cmd, out, &n, err, sizeof err) == 0 && n == wn && memcmp(out, want, wn) == 0;
}
static int refused(const char *cmd) { uint8_t out[LJ_EXTAPP_MAX_BYTES]; size_t n; char err[64]; return lj_extapp_encode(cmd, out, &n, err, sizeof err) == -1 && err[0]; }

static void write_file(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }

static void grammar(void)
{
    char s33[34];
    CHECK(enc("TRIG!1", "TRIG", 4) && enc("TRIG!2", "TRIG", 4));
    CHECK(enc("#X7F", "\x7F", 1) && enc("#x7f", "\x7F", 1) && enc("#XFF#X00", "\xFF\x00", 2));
    CHECK(enc("#123#1#2#3", "\x7B\x01\x02\x03", 4) && enc("Test#1#2#3", "Test\x01\x02\x03", 7) && enc("#255", "\xFF", 1));
    CHECK(enc("Command123", "Command123", 10) && enc("Te st", "Te st", 5) && enc("Test  ", "Test", 4) && enc("Test !", "Test ", 5));   /* RS232Send help examples */
    CHECK(enc("Test#35", "Test#", 5) && enc("", "", 0) && enc("!only", "", 0) && enc("\x7F", "\x7F", 1));
    CHECK(refused("#256") && refused("#X100") && refused("#") && refused("#X") && refused("#!") && refused("A#B"));
    memset(s33, 'A', 33); s33[33] = 0;
    CHECK(refused(s33) && !refused(s33 + 1));                       /* 33 characters refused, 32 accepted */
    CHECK(refused("caf\xE9") && refused("tab\there") && lj_extapp_validate("ok") == NULL);
    { uint32_t ip = 0; CHECK(lj_net_ip_parse("127.0.0.1", &ip) == 0 && ip == 0x7F000001u);
      CHECK(lj_net_ip_parse("256.1.1.1", &ip) && lj_net_ip_parse("1.2.3", &ip) && lj_net_ip_parse("1.2.3.4.5", &ip) && lj_net_ip_parse("1..2.3", &ip) && lj_net_ip_parse("host", &ip) && lj_net_ip_parse("0001.2.3.4", &ip)); }
    printf("  grammar: TRIG!1 / TRIG!2 -> TRIG, #X7F -> 7F, #256 and 33 characters refused\n");
}

static void settings(void)
{
    lj_ini *ini = lj_ini_new();
    lj_extapp_cfg c, d;
    lj_extapp_link l;
    char err[96];
    lj_ini *def;
    lj_extapp_cfg_default(&c);
    c.link[0].fixture = 5; c.link[0].transport = LJ_EXTAPP_TCP; c.link[0].ip = LJ_IP_LOOPBACK; c.link[0].port = 7000; strcpy(c.link[0].def_file, "x.Ini");
    c.link[1].fixture = 9; c.link[1].transport = LJ_EXTAPP_SERIAL; strcpy(c.link[1].serial_port, "COM3"); c.link[1].baud = 19200;
    lj_extapp_cfg_save(ini, &c);
    lj_extapp_cfg_load(ini, &d);
    CHECK(memcmp(&c, &d, sizeof c) == 0);                           /* round trip */
    lj_ini_seti(ini, "EXTAPP", "Fixture3", 5);                      /* hostile: a fixture twice, out-of-range numbers */
    lj_ini_seti(ini, "EXTAPP", "Transport3", 2); lj_ini_sets(ini, "EXTAPP", "Host3", "127.0.0.1"); lj_ini_seti(ini, "EXTAPP", "Port3", 1);
    lj_ini_seti(ini, "EXTAPP", "Fixture4", 101); lj_ini_seti(ini, "EXTAPP", "Transport2", 7); lj_ini_seti(ini, "EXTAPP", "Baud1", -5);
    lj_ini_sets(ini, "EXTAPP", "Port1", "99999");
    lj_extapp_cfg_load(ini, &d);
    CHECK(d.link[2].fixture == 0 && d.link[3].fixture == 0 && d.link[1].transport == LJ_EXTAPP_OFF && d.link[0].transport == LJ_EXTAPP_OFF && d.link[0].baud == LJ_EXTAPP_BAUD_DEFAULT);
    lj_extapp_cfg_load(NULL, &d);
    CHECK(d.link[0].fixture == 0 && d.link[0].transport == LJ_EXTAPP_OFF);   /* fresh Hardware.Ini: everything off (W5-X-OFF) */
    lj_ini_free(ini);

    write_file(TMP "tcp.Ini", "[IDENTIFIERS]\nNAME=Test\n[LJ4EVR]\nTRANSPORT=TCP\nHOST=127.0.0.1\nPORT=7001\n");
    def = lj_extapp_def_open(TMP "tcp.Ini", err, sizeof err);
    memset(&l, 0, sizeof l);
    CHECK(def && lj_extapp_def_link(def, &l, err, sizeof err) == 0 && l.transport == LJ_EXTAPP_TCP && l.ip == LJ_IP_LOOPBACK && l.port == 7001);
    lj_ini_free(def);
    write_file(TMP "bad.Ini", "[LJ4EVR]\nTRANSPORT=TCP\nHOST=localhost\nPORT=7001\n");
    def = lj_extapp_def_open(TMP "bad.Ini", err, sizeof err);
    CHECK(def && lj_extapp_def_link(def, &l, err, sizeof err) == -1 && l.transport == LJ_EXTAPP_OFF && strstr(err, "HOST"));
    lj_ini_free(def);
    write_file(TMP "ser.Ini", "[LJ4EVR]\nTRANSPORT=SERIAL\nSERIALPORT=COM2\nBAUD=9999999\n");
    def = lj_extapp_def_open(TMP "ser.Ini", err, sizeof err);
    CHECK(def && lj_extapp_def_link(def, &l, err, sizeof err) == -1 && strstr(err, "BAUD"));
    lj_ini_free(def);
    write_file(TMP "rs232.Ini", "[IDENTIFIERS]\nCOMMANDINTERFACE=ZSTRING\n[PORTSETUP]\nPORT=1\n");   /* an LJ2 plug-in file: shown, sends nothing */
    def = lj_extapp_def_open(TMP "rs232.Ini", err, sizeof err);
    CHECK(def && lj_extapp_def_link(def, &l, err, sizeof err) == -1 && l.transport == LJ_EXTAPP_OFF);
    lj_ini_free(def);
    {   /* a definition file over 16 KB is refused before it is parsed */
        FILE *f = fopen(TMP "big.Ini", "wb");
        int i;
        for (i = 0; f && i < 2000; i++) fputs("[S]\nKEY=0123456789\n", f);
        if (f) fclose(f);
        CHECK(lj_extapp_def_open(TMP "big.Ini", err, sizeof err) == NULL && strstr(err, "16 KB"));
        CHECK(lj_extapp_def_open(TMP "missing.Ini", err, sizeof err) == NULL);
    }
    printf("  settings: [EXTAPP] round trip, hostile keys clamped, definition files checked\n");
}

static void engine_copy(void)
{
    static lj_sequence q, b; static lj_scene sq[1], sb[1];
    lj_show *show = lj_show_new();
    lj_engine *e = lj_engine_create();
    lj_bgcue bg;
    lj_cmd c;
    const lj_frame *f;
    uint64_t now = 1000000;
    if (!show || !e) { fails++; printf("FAIL engine setup\n"); return; }
    show->fixture[4].kind = LJ_FX_BUILTIN; show->fixture[4].profile_id = 90; show->fixture[4].link = 2; show->fixture[4].address = 1;
    show->fixture[6].kind = LJ_FX_BUILTIN; show->fixture[6].profile_id = 90; show->fixture[6].link = 2; show->fixture[6].address = 33;
    show->fixture[7].kind = LJ_FX_BUILTIN; show->fixture[7].profile_id = 19; show->fixture[7].link = 1; show->fixture[7].address = 1;   /* a dimmer: never copied */
    memset(&q, 0, sizeof q); memset(sq, 0, sizeof sq); q.number = 1; q.n = 1; q.scenes = sq; sq[0].scene_ms = 1000;
    memset(&b, 0, sizeof b); memset(sb, 0, sizeof sb); b.number = 2; b.n = 1; b.scenes = sb; sb[0].scene_ms = 1000;
    sq[0].ext[0].fixture = 5; strcpy(sq[0].ext[0].cmd, "TRIG!1");
    sq[0].ext[1].fixture = 8; strcpy(sq[0].ext[1].cmd, "DIM");
    sb[0].ext[0].fixture = 5; strcpy(sb[0].ext[0].cmd, "BG");    /* BG slots merge above the current sequence: BG wins fixture 5 */
    sb[0].ext[1].fixture = 7; strcpy(sb[0].ext[1].cmd, "SEVEN");
    lj_lock(); lj_engine_bind(e, show); lj_unlock();
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_CURRENT_SEQ; c.a = 0; c.seq[0] = &q;
    lj_lock(); lj_engine_cmd(e, &c); lj_engine_tick(e, now += 25000); f = lj_engine_frame(e); lj_unlock();
    CHECK(f->ext_fixture[0] == 5 && strcmp(f->ext[0], "TRIG!1") == 0 && f->ext_fixture[1] == 0);   /* fixture 8 is a dimmer */
    memset(&bg, 0, sizeof bg); bg.number = 1; bg.slot[0].seq = 2; bg.slot[0].enabled = 1;
    memset(&c, 0, sizeof c); c.kind = LJ_CMD_LOAD_BG; c.bg = &bg; c.bgseq[0] = &b;
    lj_lock(); lj_engine_cmd(e, &c); lj_engine_tick(e, now += 25000); f = lj_engine_frame(e); lj_unlock();
    CHECK(f->ext_fixture[0] == 5 && strcmp(f->ext[0], "BG") == 0 && f->ext_fixture[1] == 7 && strcmp(f->ext[1], "SEVEN") == 0);
    lj_lock(); lj_engine_bind(e, NULL); lj_unlock();
    lj_engine_destroy(e);
    lj_show_free(show);
    printf("  engine: frame.ext carries the topmost runner's string per profile-90 fixture\n");
}

/* W6 LORD_ENGINE-8: the statics layer their strings in activation order (LJ_LAYER_ORDER and the lj_merge statics sort),
 * not in runner order. A on runner 0 and B on runner 1 both name a string for fixture 5: B, activated later, wins; A
 * re-activated (same runner, now the latest) wins back. */
static void engine_statics(void)
{
    static lj_static_slot a, b; static lj_scene sa, sb;
    lj_show *show = lj_show_new();
    lj_engine *e = lj_engine_create();
    lj_cmd c;
    const lj_frame *f;
    uint64_t now = 1000000;
    int i;
    if (!show || !e) { fails++; printf("FAIL engine setup\n"); lj_show_free(show); lj_engine_destroy(e); return; }
    show->fixture[4].kind = LJ_FX_BUILTIN; show->fixture[4].profile_id = 90; show->fixture[4].link = 2; show->fixture[4].address = 1;
    memset(&sa, 0, sizeof sa); memset(&sb, 0, sizeof sb);
    sa.ext[0].fixture = 5; strcpy(sa.ext[0].cmd, "STATIC A");
    sb.ext[0].fixture = 5; strcpy(sb.ext[0].cmd, "STATIC B");
    memset(&a, 0, sizeof a); a.kind = LJ_STATIC_RECORDED; a.level_pct = 100; a.replace = LJ_REP_ADD; a.scene = &sa;
    b = a; b.scene = &sb;
    lj_lock(); lj_engine_bind(e, show); lj_unlock();
    for (i = 0; i < 3; i++) {                                    /* A, then B, then A again */
        memset(&c, 0, sizeof c); c.kind = LJ_CMD_STATIC_ACTIVATE; c.a = i == 1 ? 1u : 0u; c.b = 0xFFFF; c.c = 0xFF; c.st = i == 1 ? &b : &a;
        lj_lock(); lj_engine_cmd(e, &c); lj_engine_tick(e, now += 25000); f = lj_engine_frame(e); lj_unlock();
        CHECK(f->ext_fixture[0] == 5 && strcmp(f->ext[0], i == 1 ? "STATIC B" : "STATIC A") == 0 && f->ext_fixture[1] == 0);
    }
    {   const lj_runner *r0 = lj_engine_runner(e, LJ_RUN_STATIC0), *r1 = lj_engine_runner(e, LJ_RUN_STATIC0 + 1);
        CHECK(r0 && r1 && lj_runner_active(r0) && lj_runner_active(r1) && r0->activated_us > r1->activated_us); }   /* the later activation sits on the lower runner */
    lj_lock(); lj_engine_bind(e, NULL); lj_unlock();
    lj_engine_destroy(e);
    lj_show_free(show);
    printf("  engine: two statics on fixture 5, the later-activated one's string wins (A, B, A again on runner 0)\n");
}

static int recv_wait(lj_socket s, uint8_t *buf, size_t cap)        /* up to ~1 s of polling */
{
    int i, n = 0;
    for (i = 0; i < 1000 && n == 0; i++) { uint64_t t = lj_clock_us(); n = lj_socket_recv(s, buf, cap); while (n == 0 && lj_clock_us() - t < 1000) {} }
    return n;
}

static lj_socket step_until_accept(lj_socket ls, uint8_t *fx, char (*ext)[LJ_EXT_CMD_CAP], uint64_t *now)
{
    lj_socket a = LJ_SOCKET_BAD;
    int i;
    for (i = 0; i < 2000 && a == LJ_SOCKET_BAD; i++) { lj_extapp_step(fx, &ext[0][0], *now += 1000); a = lj_socket_accept(ls, NULL); }
    for (i = 0; i < 200; i++) lj_extapp_step(fx, &ext[0][0], *now += 1000);   /* the client sees its connect complete and sends */
    return a;
}

static void loopback(void)
{
    lj_socket ls = lj_socket_listen(LJ_IP_LOOPBACK, 0, 2), a;
    lj_extapp_cfg c;
    lj_extapp_stats st;
    uint8_t fx[LJ_SCENE_EXT] = { 5, 0, 0, 0 }, buf[64];
    char ext[LJ_SCENE_EXT][LJ_EXT_CMD_CAP];
    uint64_t now = 1000000;
    int n;
    CHECK(ls != LJ_SOCKET_BAD);
    if (ls == LJ_SOCKET_BAD) return;
    memset(ext, 0, sizeof ext);
    lj_extapp_cfg_default(&c);
    c.link[0].fixture = 5; c.link[0].transport = LJ_EXTAPP_TCP; c.link[0].ip = LJ_IP_LOOPBACK; c.link[0].port = lj_socket_port(ls);
    lj_extapp_open(&c);
    lj_extapp_step(fx, &ext[0][0], now);                            /* "" : nothing to send, nothing opened */
    CHECK(lj_socket_accept(ls, NULL) == LJ_SOCKET_BAD);
    strcpy(ext[0], "TRIG!1");
    a = step_until_accept(ls, fx, ext, &now);
    CHECK(a != LJ_SOCKET_BAD);
    n = recv_wait(a, buf, sizeof buf);
    CHECK(n == 4 && memcmp(buf, "TRIG", 4) == 0);
    lj_extapp_step(fx, &ext[0][0], now += 25000);                  /* unchanged: nothing */
    strcpy(ext[0], "TRIG!2");
    lj_extapp_step(fx, &ext[0][0], now += 25000);
    n = recv_wait(a, buf, sizeof buf);
    CHECK(n == 4 && memcmp(buf, "TRIG", 4) == 0);                  /* the same bytes again: the suffix changed */
    strcpy(ext[0], "#X7F#256");                                     /* refused, counted, nothing sent */
    lj_extapp_step(fx, &ext[0][0], now += 25000);
    lj_extapp_stats_get(&st);
    CHECK(st.sent == 2 && st.refused == 1 && st.failed == 0 && st.connected[0] == 1);
    lj_socket_close(a);                                             /* the peer dies */
    { uint64_t t = lj_clock_us(); while (lj_clock_us() - t < 100000u) {} }   /* let the FIN arrive (macOS loopback lags; a send before it is lost, as on any TCP) */
    strcpy(ext[0], "GO");
    { int i; for (i = 0; i < 200; i++) lj_extapp_step(fx, &ext[0][0], now += 1000); }
    lj_extapp_stats_get(&st);
    CHECK(st.failed >= 1 && st.connected[0] == 0 && st.sent == 2);  /* counted; the loop goes on */
    now += LJ_EXTAPP_RECONNECT_US;                                  /* the reconnect rule: the latest string goes out on the new link */
    a = step_until_accept(ls, fx, ext, &now);
    CHECK(a != LJ_SOCKET_BAD);
    n = recv_wait(a, buf, sizeof buf);
    CHECK(n == 2 && memcmp(buf, "GO", 2) == 0);
    lj_extapp_stats_get(&st);
    CHECK(st.sent == 3 && st.connected[0] == 1);
    lj_extapp_close();
    lj_socket_close(a); lj_socket_close(ls);
    printf("  loopback TCP: TRIG twice, #256 refused, dead peer counted (failed=%u), reconnect delivers GO\n", (unsigned)st.failed);
}

int lj_test_extapp(int argc, char **argv);
int lj_test_extapp(int argc, char **argv)
{
    (void)argc; (void)argv;
    fails = 0;
    lj_fs_mkdir(TMP);
    grammar(); settings(); engine_copy(); engine_statics(); loopback();
    printf("test_extapp: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

int lj_test_extapp_seq(int argc, char **argv);
int lj_test_extapp_seq(int argc, char **argv)
{
    lj_sequence s, t;
    char err[256];
    (void)argc; (void)argv;
    fails = 0;
    lj_fs_mkdir(TMP);
    memset(&s, 0, sizeof s); memset(&t, 0, sizeof t);
    lj_sequence_new(&s);
    lj_sequence_alloc(&s, 2);
    s.number = 12;
    s.scenes[0].ext[0].fixture = 5; strcpy(s.scenes[0].ext[0].cmd, "TRIG!1");
    s.scenes[1].ext[2].fixture = 100; strcpy(s.scenes[1].ext[2].cmd, "say \"hi\" #13");
    s.scenes[1].val[3] = 200; s.scenes[1].state[3] = LJ_SNAP;
    CHECK(lj_sequence_save(TMP "00000012.seq", &s) == 0);
    CHECK(lj_sequence_load(TMP "00000012.seq", &t, err, sizeof err) == 0);
    CHECK(t.n == 2 && t.scenes[0].ext[0].fixture == 5 && strcmp(t.scenes[0].ext[0].cmd, "TRIG!1") == 0);
    CHECK(t.scenes[1].ext[0].fixture == 100 && strcmp(t.scenes[1].ext[0].cmd, "say \"hi\" #13") == 0 && t.scenes[1].val[3] == 200);   /* slots compact on load */
    lj_sequence_free_scenes(&t);
    write_file(TMP "v100.seq", "LJ4 seq 1\nnum=3\nname=Old\ntype=\nsync=0 gst=0 gft=0\nscenes=1\nscene st=1000 ft=50\nch 1:1=255:S\n");   /* v1.0.0: no ext */
    CHECK(lj_sequence_load(TMP "v100.seq", &t, err, sizeof err) == 0 && t.scenes[0].ext[0].fixture == 0 && t.scenes[0].val[0] == 255);
    lj_sequence_free_scenes(&t);
    {
        static const char *const bad[] = { "ext 0 cmd=A", "ext 101 cmd=A", "ext 5", "ext 5 cmd=", "ext 5 cmd=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" /* 33 */,
                                           "ext 5 cmd=\"unterminated", "ext 5 cmd=caf\xE9", "ext 99999999999999999999 cmd=A" };
        char text[512];
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            if (snprintf(text, sizeof text, "LJ4 seq 1\nnum=3\nscenes=1\nscene st=1000 ft=50\n%s\n", bad[i]) < 0) text[0] = 0;
            write_file(TMP "bad.seq", text);
            CHECK(lj_sequence_load(TMP "bad.seq", &t, err, sizeof err) == -1);
        }
        write_file(TMP "bad.seq", "LJ4 seq 1\nnum=3\nscenes=1\nscene st=1000 ft=50\next 5 cmd=A\next 5 cmd=B\n");   /* twice for one fixture */
        CHECK(lj_sequence_load(TMP "bad.seq", &t, err, sizeof err) == -1);
        write_file(TMP "bad.seq", "LJ4 seq 1\nnum=3\nscenes=1\next 5 cmd=A\nscene st=1000 ft=50\n");                   /* before the first scene */
        CHECK(lj_sequence_load(TMP "bad.seq", &t, err, sizeof err) == -1);
        write_file(TMP "bad.seq", "LJ4 seq 1\nnum=3\nscenes=1\nscene st=1000 ft=50\next 1 cmd=A\next 2 cmd=B\next 3 cmd=C\next 4 cmd=D\next 6 cmd=E\n");   /* a fifth */
        CHECK(lj_sequence_load(TMP "bad.seq", &t, err, sizeof err) == -1);
    }
    lj_sequence_free_scenes(&s);
    printf("test_extapp_seq: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int rc = fa && fb, ca = 0, cb = 0;
    while (rc && ca != EOF) { ca = fgetc(fa); cb = fgetc(fb); rc = ca == cb; }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return rc;
}
static void golden(lj_window *w, const char *name)
{
    char path[1024];
    const char *upd = getenv("LJ_GOLDEN_UPDATE");
    if (!w) { fails++; printf("FAIL golden %s: no window\n", name); return; }
    if (snprintf(path, sizeof path, "%s/tests/ui/golden/%s.bmp", LJ_SOURCE_DIR, name) < 0) path[0] = 0;
    lj_window_paint(w);
    if (upd && upd[0] == '1') { CHECK(lj_bmp_write(lj_window_surface(w), path) == 0); printf("  golden %s written\n", name); return; }
    CHECK(lj_bmp_write(lj_window_surface(w), TMP "golden.bmp") == 0);
    if (!same_file(TMP "golden.bmp", path)) { fails++; printf("FAIL golden %s differs (new render at " TMP "golden.bmp)\n", name); }
    else printf("  golden %s: identical\n", name);
}

static void click(lj_window *w, const char *name)
{
    lj_widget *b = lj_ctl_of(w) ? lj_ctl_elem_find(w, name) : lj_window_find(w, name);
    CHECK(b != NULL);
    if (b && b->on_event) b->on_event(b, LJ_WEV_CLICK, b->ud);
}

int lj_test_extapp_ui(int argc, char **argv);
int lj_test_extapp_ui(int argc, char **argv)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_show *show = lj_show_new();
    lj_window *w;
    lj_widget *ed;
    lj_ini *hw = lj_ini_new();
    (void)argc; (void)argv;
    fails = 0;
    lj_fs_mkdir(TMP);
    if (!show) return 1;
    show->fixture[4].kind = LJ_FX_BUILTIN; show->fixture[4].profile_id = 90; show->fixture[4].link = 2; show->fixture[4].address = 1;   /* "External Application/Device" */
    show->current_seq = lj_sequence_new_rec();
    ctx->show = show; ctx->current_scene = 0; ctx->hardware_ini = hw; ctx->engine = NULL;
    lj_str_copy(ctx->userdir, sizeof ctx->userdir, TMP);
    lj_app_io_set_platform(NULL);
    lj_app_io_start();                                              /* binds LJ_ACT_EXTAPP_SETUP / LJ_ACT_IO_APPLY, ctx->extapp */
    lj_sel_reset();
    lj_sel_set(4, 1, 1);

    /* entry 1: Fixture toolbar button 7 -> the Extended window of profile 90 */
    w = lj_ctl_open(LJ_CTL_EXTENDED);
    CHECK(w != NULL);
    if (w) {
        CHECK(strcmp(lj_window_caption(w)->text, "External Application") == 0);
        CHECK(strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_box")), "ZString Interface") == 0);
        CHECK(strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_label")), "External Application Command") == 0);
        CHECK(strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_set")), "Set Command") == 0 && strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_get")), "Get Command") == 0);
        CHECK(strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_link")), "Application Link Undefined") == 0);
        ed = lj_ctl_elem_find(w, "ext_cmd");
        lj_w_edit_set_text(ed, "TRIG!1");
        click(w, "ext_set");
        CHECK(show->current_seq->scenes[0].ext[0].fixture == 5 && strcmp(show->current_seq->scenes[0].ext[0].cmd, "TRIG!1") == 0 && show->current_seq->modified);
        lj_w_edit_set_text(ed, "#256");                             /* refused at the edit: the scene keeps TRIG!1 */
        click(w, "ext_set");
        CHECK(strcmp(show->current_seq->scenes[0].ext[0].cmd, "TRIG!1") == 0);
        lj_w_edit_set_text(ed, "");
        click(w, "ext_get");
        CHECK(strcmp(lj_w_edit_text(ed), "TRIG!1") == 0);
        golden(w, "CTL_EXTERNAL_EXTENDED");
        printf("PASS entry Fixture toolbar button 7 -> TSpecialSettings_ExternalApp (Set / Get Command)\n");
    }

    /* entry 2: icon popup Define Application Link -> TExternalAppSetupForm */
    write_file(TMP "dev.Ini", "[IDENTIFIERS]\nTYPE=LIGHTJOCKEY EXTERNAL APPLICATION LINK\nNAME=Loopback test\nCOMMANDINTERFACE=ZSTRING\nFULLUPDATE=0\nDMXOUTCHANNELS=0\n[LJ4EVR]\nTRANSPORT=TCP\nHOST=127.0.0.1\nPORT=7002\n");
    lj_desktop_run_action(LJ_ACT_EXTAPP_SETUP, 4);
    w = lj_win_get(LJ_WIN_EXTAPP_SETUP);
    CHECK(w != NULL);
    if (w) {
        lj_widget *lv = lj_window_find(w, "lv1");
        CHECK(strcmp(lj_window_caption(w)->text, "External Application Link") == 0);
        CHECK(!lj_window_find(w, "cbAutoLaunch")->enabled && !lj_window_find(w, "seDMXIn4")->enabled && lj_window_find(w, "bOk")->enabled);
        CHECK(lv && ((lj_w_listview *)lv)->n == 1);                  /* "Definition File / Application Link Undefined" */
        lj_w_edit_set_text(lj_window_find(w, "eFileName"), TMP "dev.Ini");
        CHECK(lv && ((lj_w_listview *)lv)->n == 8);                 /* Definition File, NAME, COMMANDINTERFACE, FULLUPDATE, DMXOUTCHANNELS, TRANSPORT, HOST, PORT */
        golden(w, "W5X_EXTAPP_SETUP");
        click(w, "bOk");
        CHECK(lj_win_get(LJ_WIN_EXTAPP_SETUP) == NULL);
        CHECK(ctx->extapp->link[0].fixture == 5 && ctx->extapp->link[0].transport == LJ_EXTAPP_TCP && ctx->extapp->link[0].port == 7002);
        CHECK(lj_ini_geti(hw, "EXTAPP", "Fixture1", 0) == 5 && lj_ini_geti(hw, "EXTAPP", "Port1", 0) == 7002);
        printf("PASS entry Define Application Link -> TExternalAppSetupForm (OK stores [EXTAPP])\n");
    }
    w = lj_ctl_window_get(LJ_CTL_EXTENDED);
    if (w) { lj_ctl_refresh(); CHECK(strcmp(lj_w_caption(lj_ctl_elem_find(w, "ext_link")), "Link:-") == 0); }   /* configured, not connected */
    lj_desktop_run_action(LJ_ACT_EXTAPP_SETUP, 4);                 /* an empty name unlinks */
    if ((w = lj_win_get(LJ_WIN_EXTAPP_SETUP)) != NULL) {
        CHECK(strcmp(lj_w_edit_text(lj_window_find(w, "eFileName")), TMP "dev.Ini") == 0);
        lj_w_edit_set_text(lj_window_find(w, "eFileName"), "");
        click(w, "bOk");
        CHECK(ctx->extapp->link[0].fixture == 0);
    }
    lj_app_io_stop();
    lj_ctl_close_all();
    lj_win_close_all();
    lj_sel_reset();
    ctx->show = NULL; ctx->hardware_ini = NULL;
    lj_show_free(show);
    lj_ini_free(hw);
    printf("test_extapp_ui: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
