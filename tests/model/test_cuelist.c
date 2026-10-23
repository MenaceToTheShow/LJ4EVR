/* test_cuelist.c - `lj4evr_tests test_cuelist`: the W3 cue list model (owner: cuelist-core; the contracts seat wrote
 * the first rows). Rows now: the command enum matches the cmd= words and both caption tables; the CD / DA / MP
 * families are inert (design 8.5) and every Wait is a wait; H:M:S:F <-> ms round-trips every frame at 24, 25, 30,
 * 75 and 100 fps; the time code text. cuelist-core rows: a 20-line list with every storable command round-trips
 * field for field and byte-equal; a .Cl2 with an MP Play line loads with that line load_error (red) and the rest
 * intact, saves byte-equal (ME-11, design 8.5); every CD / DA / MP word and an unknown word are kept and written back
 * as read; out-of-range values refuse the file; set_current_cue / set_cue_trig rejected as stored cmds by validate. */
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "model/lj_cuelist.h"

static int fails;
#define CHECK(cond) do { if (!(cond)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void lj_t_write(const char *path, const char *text) { FILE *f = fopen(path, "wb"); if (f) { fputs(text, f); fclose(f); } }
static long lj_t_read(const char *path, char *buf, long cap)
{
    FILE *f = fopen(path, "rb"); long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)cap - 1, f); fclose(f); buf[n] = 0;
    return n;
}
static int lj_t_same_file(const char *a, const char *b)
{
    static char x[65536], y[65536];
    long n = lj_t_read(a, x, sizeof x), m = lj_t_read(b, y, sizeof y);
    return n > 0 && n == m && memcmp(x, y, (size_t)n) == 0;
}
static lj_cl_line lj_t_line(int manual, int cmd, uint32_t param, const char *pstr, uint32_t ref, uint32_t cue, uint16_t trig, const char *comment)
{
    lj_cl_line l;
    memset(&l, 0, sizeof l);
    l.manual = (uint8_t)manual; l.cmd = (uint8_t)cmd; l.param_ms = param; l.ref = ref; l.cue = cue; l.trig_mask = trig;
    snprintf(l.param, sizeof l.param, "%s", pstr); snprintf(l.comment, sizeof l.comment, "%s", comment);
    l.load_error = (uint8_t)lj_cl_cmd_inert(cmd);
    return l;
}

/* the 20-line list: every storable command once, cues 1..4, a Set Cue Trig line, two manual lines, marks, tracking */
static void lj_t_list20(lj_cuelist *cl)
{
    lj_cl_line rows[20];
    int i;
    rows[0]  = lj_t_line(0, LJ_CL_NONE,             0,        "",                   0, 1, 0,     "opening");
    rows[1]  = lj_t_line(0, LJ_CL_WAIT_ELAPSED,     2000,     "",                   0, 2, 0,     "");
    rows[2]  = lj_t_line(1, LJ_CL_WAIT_ELAPSED,     5000,     "",                   0, 3, 0,     "manual: wait ignored");
    rows[3]  = lj_t_line(0, LJ_CL_WAIT_PC_CLOCK,    lj_cl_hmsf_ms(20, 30, 0, 0, 100), "", 0, 0, 0, "");
    rows[4]  = lj_t_line(0, LJ_CL_WAIT_DATE_CHANGE, 0,        "",                   0, 0, 0,     "");
    rows[5]  = lj_t_line(0, LJ_CL_WAIT_MTC,         lj_cl_hmsf_ms(1, 0, 0, 12, 25), "25", 0, 4, 0, "");
    rows[6]  = lj_t_line(0, LJ_CL_AUTOSEEK_MTC,     0,        "",                   0, 0, 0,     "");
    rows[7]  = lj_t_line(0, LJ_CL_EXECUTE_FILE,     0,        "C:\\Show Files\\fog.exe", 0, 0, 0, "/on \"quoted\"");
    rows[8]  = lj_t_line(0, LJ_CL_HOTKEY_COMMAND,   42,       "",                   0, 0, 0,     "");
    rows[9]  = lj_t_line(0, LJ_CL_FLASH_SEQUENCE,   99900,    "",                   7, 0, 0,     "");
    rows[10] = lj_t_line(0, LJ_CL_LOAD_BACKGROUND_CUE, 0,     "",                   3, 0, 0,     "");
    rows[11] = lj_t_line(0, LJ_CL_NONE,             0,        "",                   0, 0, 0x003, "Set Cue Trig 1+2");
    rows[12] = lj_t_line(1, LJ_CL_NONE,             0,        "",                   0, 2, 0,     "");
    rows[13] = lj_t_line(0, LJ_CL_WAIT_ELAPSED,     lj_cl_hmsf_ms(0, 99, 59, 99, 100), "", 0, 0, 0, "max elapsed");
    rows[14] = lj_t_line(0, LJ_CL_NONE,             0,        "",                   0, 99999999, 0, "");
    rows[15] = lj_t_line(0, LJ_CL_NONE,             0,        "",                   0, 0, 0xFFF, "");
    rows[16] = lj_t_line(0, LJ_CL_WAIT_ELAPSED,     10,       "",                   0, 1, 0,     "caf\xe9 CP1252");
    rows[17] = lj_t_line(0, LJ_CL_LINK_CUELIST,     0,        "",                   12, 0, 0,    "");
    rows[18] = lj_t_line(0, LJ_CL_GOTO_LINE,        3,        "",                   0, 0, 0,     "loop");
    rows[19] = lj_t_line(0, LJ_CL_GOTO_LINE,        21,       "",                   0, 0, 0,     "beyond: ends");
    memset(cl, 0, sizeof *cl);
    cl->number = 7; snprintf(cl->name, sizeof cl->name, "Main show"); cl->tracking = 1;
    cl->mark[0] = 3; cl->mark[3] = 20;
    for (i = 0; i < 20; i++) lj_cuelist_insert(cl, (uint32_t)i, &rows[i]);
}

static int lj_t_line_eq(const lj_cl_line *a, const lj_cl_line *b)
{
    return a->line == b->line && a->manual == b->manual && a->cmd == b->cmd && a->param_ms == b->param_ms && strcmp(a->param, b->param) == 0
        && a->cue == b->cue && a->trig_mask == b->trig_mask && strcmp(a->comment, b->comment) == 0 && a->ref == b->ref && a->load_error == b->load_error;
}

static void lj_t_files(void)
{
    lj_cuelist a, b;
    char err[256];
    uint32_t i;
    int k;
    memset(&b, 0, sizeof b);
    lj_t_list20(&a);
    CHECK(a.n == 20 && lj_cuelist_validate(&a) == NULL);
    CHECK(lj_cuelist_save("test_cl_20.Cl2", &a) == 0);
    CHECK(lj_cuelist_load("test_cl_20.Cl2", &b, err, sizeof err) == 0);
    if (err[0]) printf("  %s\n", err);
    CHECK(b.n == 20 && b.number == 7 && strcmp(b.name, "Main show") == 0 && b.tracking == 1 && b.mark[0] == 3 && b.mark[3] == 20 && b.mark[1] == 0);
    for (i = 0; i < 20 && i < b.n; i++) if (!lj_t_line_eq(&a.lines[i], &b.lines[i])) { CHECK(0); printf("  line %u differs\n", (unsigned)i + 1); }
    CHECK(lj_cuelist_save("test_cl_20b.Cl2", &b) == 0 && lj_t_same_file("test_cl_20.Cl2", "test_cl_20b.Cl2"));

    /* DA Play on line 3 (design 10 W3 acceptance 2, ME-11; W5: MP is live, DA stays inert): kept, flagged, the rest intact, written back unchanged */
    a.lines[2] = lj_t_line(0, LJ_CL_DA_PLAY, 1500, "clip.wav", 0, 0, 0, "media"); a.lines[2].line = 3;
    CHECK(a.lines[2].load_error == 1 && lj_cuelist_validate(&a) == NULL);
    CHECK(lj_cuelist_save("test_cl_mp.Cl2", &a) == 0);
    CHECK(lj_cuelist_load("test_cl_mp.Cl2", &b, err, sizeof err) == 0);
    CHECK(b.n == 20 && b.lines[2].cmd == LJ_CL_DA_PLAY && b.lines[2].load_error == 1 && b.lines[2].param_ms == 1500 && strcmp(b.lines[2].param, "clip.wav") == 0);
    for (i = 0; i < b.n; i++) CHECK(b.lines[i].load_error == (i == 2));
    for (i = 0; i < 20 && i < b.n; i++) if (i != 2 && !lj_t_line_eq(&a.lines[i], &b.lines[i])) { CHECK(0); printf("  line %u differs after the MP line\n", (unsigned)i + 1); }
    CHECK(lj_cl_line_state(NULL, &b, 3) == LJ_CLL_ERROR && lj_cl_line_state(NULL, &b, 2) == LJ_CLL_PLAIN);
    CHECK(lj_cuelist_save("test_cl_mp2.Cl2", &b) == 0 && lj_t_same_file("test_cl_mp.Cl2", "test_cl_mp2.Cl2"));

    /* every command word once and an unknown word: CD / DA / MP and the panel actions load flagged, all write back as read */
    {
        static char text[16384];
        size_t n = (size_t)snprintf(text, sizeof text, "LJ4 cl 1\nnum=9\nname=all\ntracking=0\n");
        for (k = 0; k < LJ_CL_CMD_COUNT; k++)
            n += (size_t)snprintf(text + n, sizeof text - n, "line %d adv=auto cmd=%s param=%d pstr=p%d ref=0 cue=0 trig=0 comment=c\n", k + 1, lj_cl_cmd_words[k], k, k);
        snprintf(text + n, sizeof text - n, "line %d adv=manual cmd=dmx_send_frame param=5 pstr=\"two words\" ref=4 cue=0 trig=0 comment=future\n", k + 1);
        lj_t_write("test_cl_all.Cl2", text);
        CHECK(lj_cuelist_load("test_cl_all.Cl2", &b, err, sizeof err) == 0);
        if (err[0]) printf("  %s\n", err);
        CHECK(b.n == (uint32_t)LJ_CL_CMD_COUNT + 1);
        for (k = 0; k < LJ_CL_CMD_COUNT && (uint32_t)k < b.n; k++)
            CHECK(b.lines[k].cmd == k && b.lines[k].load_error == (lj_cl_cmd_inert(k) || k == LJ_CL_SET_CURRENT_CUE || k == LJ_CL_SET_CUE_TRIG || k == LJ_CL_MP_LOAD_FILE /* W5: its clip p12 is not there */));
        if (b.n == (uint32_t)LJ_CL_CMD_COUNT + 1) {
            const lj_cl_line *u = &b.lines[LJ_CL_CMD_COUNT];
            CHECK(u->cmd == LJ_CL_NONE && u->load_error == 1 && u->manual == 1 && strcmp(u->param, "dmx_send_frame") == 0 && strcmp(u->param + strlen(u->param) + 1, "two words") == 0);
            CHECK(lj_cl_line_state(NULL, &b, (uint32_t)LJ_CL_CMD_COUNT + 1) == LJ_CLL_ERROR);
        }
        CHECK(lj_cuelist_save("test_cl_all2.Cl2", &b) == 0 && lj_t_same_file("test_cl_all.Cl2", "test_cl_all2.Cl2"));
    }

    /* ME-107: a 127-char Execute File pstr (the param cap) round-trips; every key after it still reads */
    a.lines[4] = lj_t_line(1, LJ_CL_EXECUTE_FILE, 0, "", 0, 0, 0, "after");
    memset(a.lines[4].param, 'p', sizeof a.lines[4].param - 1); a.lines[4].param[sizeof a.lines[4].param - 1] = 0; a.lines[4].line = 5;
    CHECK(lj_cuelist_save("test_cl_long.Cl2", &a) == 0);
    CHECK(lj_cuelist_load("test_cl_long.Cl2", &b, err, sizeof err) == 0 && b.n == 20 && lj_t_line_eq(&a.lines[4], &b.lines[4]));
    remove("test_cl_long.Cl2");

    /* file boundary: out-of-range values refuse the file (never alias into range) */
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=1\nline 1 adv=auto cmd=none trig=4096\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1 && b.n == 0);
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=1\nline 1 adv=auto cmd=wait_pc_clock param=86400000\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1);
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=100000000\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1);
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=1\nline 1 adv=auto cmd=flash_sequence param=99901 ref=1\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1);
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=1\nline 1 adv=auto cmd=this_command_word_is_far_too_long_to_be_one\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1);
    lj_t_write("test_cl_bad.Cl2", "LJ4 cl 1\nnum=1\nline 2 adv=auto cmd=none\n");
    CHECK(lj_cuelist_load("test_cl_bad.Cl2", &b, err, sizeof err) == -1);

    /* validate: the panel actions are not stored commands; an inert command needs its flag */
    a.lines[0].cmd = LJ_CL_SET_CURRENT_CUE; CHECK(lj_cuelist_validate(&a) != NULL);
    a.lines[0].cmd = LJ_CL_SET_CUE_TRIG;    CHECK(lj_cuelist_validate(&a) != NULL);
    a.lines[0].load_error = 1;              CHECK(lj_cuelist_validate(&a) == NULL);
    a.lines[0].cmd = LJ_CL_CD_STOP; a.lines[0].load_error = 0; CHECK(lj_cuelist_validate(&a) != NULL);
    a.lines[0].cmd = LJ_CL_NONE; a.lines[0].cue = 1; a.lines[0].trig_mask = 1; CHECK(lj_cuelist_validate(&a) != NULL);
    free(a.lines); free(b.lines);
    remove("test_cl_20.Cl2"); remove("test_cl_20b.Cl2"); remove("test_cl_mp.Cl2"); remove("test_cl_mp2.Cl2");
    remove("test_cl_all.Cl2"); remove("test_cl_all2.Cl2"); remove("test_cl_bad.Cl2");
}

int lj_test_cuelist(int argc, char **argv);
int lj_test_cuelist(int argc, char **argv)
{
    static const int fps[] = { 24, 25, 30, 75, 100 };
    char t[16];
    int i, k, f, h, m, s, ff, n = 0;
    (void)argc; (void)argv;
    while (lj_cl_cmd_words[n]) n++;
    CHECK(n == LJ_CL_CMD_COUNT);
    CHECK(strcmp(lj_cl_cmd_words[LJ_CL_MP_PLAY], "mp_play") == 0 && strcmp(lj_cl_cmd_caption[LJ_CL_MP_PLAY], "MP Play") == 0);
    CHECK(strcmp(lj_cl_cmd_words[LJ_CL_LINK_CUELIST], "link_cuelist") == 0 && strcmp(lj_cl_cmd_caption[LJ_CL_GOTO_LINE], "Goto Line#") == 0);
    CHECK(strcmp(lj_cl_cmd_grid[LJ_CL_WAIT_PC_CLOCK], "Wait PC-Clock at") == 0);
    for (i = 0; i < LJ_CL_CMD_COUNT; i++) {
        int fam = lj_cl_cmd_family(i);
        CHECK(lj_cl_cmd_caption[i] != NULL && lj_cl_cmd_grid[i] != NULL);
        CHECK(lj_cl_cmd_inert(i) == (fam == LJ_CLF_CD || fam == LJ_CLF_DA));   /* W5 media: MP left the rule (ME-11 narrowed) */
    }
    CHECK(lj_cl_cmd_inert(LJ_CL_WAIT_CD) && lj_cl_cmd_inert(LJ_CL_DA_TIMECODE_FACTOR) && !lj_cl_cmd_inert(LJ_CL_MP_CLOSE));
    CHECK(!lj_cl_cmd_inert(LJ_CL_WAIT_MTC) && !lj_cl_cmd_inert(LJ_CL_WAIT_ELAPSED) && !lj_cl_cmd_inert(LJ_CL_NONE));
    CHECK(lj_cl_cmd_is_wait(LJ_CL_WAIT_ELAPSED) && lj_cl_cmd_is_wait(LJ_CL_WAIT_DATE_CHANGE) && lj_cl_cmd_is_wait(LJ_CL_MP_WAIT));
    CHECK(!lj_cl_cmd_is_wait(LJ_CL_GOTO_LINE) && !lj_cl_cmd_is_wait(LJ_CL_AUTOSEEK_MTC));
    CHECK(lj_cl_cmd_family(-1) == -1 && lj_cl_cmd_family(LJ_CL_CMD_COUNT) == -1);
    for (k = 0; k < (int)(sizeof fps / sizeof fps[0]); k++)
        for (f = 0; f < fps[k]; f++) {
            lj_cl_ms_hmsf(lj_cl_hmsf_ms(23, 59, 58, f, fps[k]), fps[k], &h, &m, &s, &ff);
            if (h != 23 || m != 59 || s != 58 || ff != f) { CHECK(0); printf("  fps %d frame %d -> %d:%d:%d:%d\n", fps[k], f, h, m, s, ff); }
        }
    CHECK(lj_cl_hmsf_ms(0, 1, 2, 50, 100) == 62500);
    CHECK(lj_cl_tc_text(t, sizeof t, -1, 25) == 0 && strcmp(t, "--:--:--:--") == 0);
    CHECK(lj_cl_tc_text(t, sizeof t, (int32_t)lj_cl_hmsf_ms(1, 2, 3, 4, 25), 25) == 0 && strcmp(t, "01:02:03:04") == 0);
    CHECK(lj_cl_tc_text(t, 4, 0, 25) == -1);
    lj_t_files();
    printf("test_cuelist: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
