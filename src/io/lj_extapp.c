/* lj_extapp.c - External Application string output (io/lj_extapp.h; note 05 s10.2, the LJ-RS232Send Command Syntax topic;
 * docs/extapp.md). UI thread only: the strings arrive as the app's copy of the engine frame (W5-X-ENGINE), leave on a
 * serial port (io/lj_serial.h, the remote track's files) or a TCP client (io/lj_socket.h). Nothing opens until a string
 * for a configured profile-90 fixture has to go out (W5-X-OFF); every handle is non-blocking (W5-X-THREAD); a failure
 * is logged once per outage and counted, never fatal. */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen under /W4 /WX on MSVC (the lj_sequence.c pattern) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io/lj_extapp.h"
#include "io/lj_serial.h"
#include "io/lj_socket.h"
#include "core/lj_log.h"
#include "core/lj_str.h"

/* ---- the grammar (05 s10.2, RS232Send "Command Syntax") ---- */

const char *lj_extapp_validate(const char *cmd)
{
    size_t n;
    if (!cmd) return "no command string";
    for (n = 0; cmd[n]; n++) {
        if (n >= LJ_EXTAPP_MAX_BYTES) return "longer than 32 characters";
        if ((unsigned char)cmd[n] < 32 || (unsigned char)cmd[n] > 127) return "a character outside ASCII 32..127";
    }
    return NULL;
}

static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

int lj_extapp_encode(const char *cmd, uint8_t out[LJ_EXTAPP_MAX_BYTES], size_t *n, char *err, size_t cap)
{
    const char *bad = lj_extapp_validate(cmd);
    size_t end = 0, i, k = 0;
    if (n) *n = 0;
    if (!bad) {
        end = strlen(cmd);
        while (end && cmd[end - 1] == ' ') end--;                   /* "trailing spaces ... are automatically removed" */
        for (i = 0; i < end && cmd[i] != '!'; ) {                   /* "!" ends the transmitted part */
            unsigned v;
            int hex, d, digits = 0;
            if (cmd[i] != '#') { out[k++] = (uint8_t)cmd[i++]; continue; }
            i++;
            hex = i < end && (cmd[i] == 'X' || cmd[i] == 'x');
            if (hex) i++;
            for (v = 0; i < end && (d = hex ? hexv(cmd[i]) : (cmd[i] >= '0' && cmd[i] <= '9' ? cmd[i] - '0' : -1)) >= 0; i++, digits++) {
                v = v * (hex ? 16u : 10u) + (unsigned)d;
                if (v > 255) break;
            }
            if (!digits) { bad = hex ? "#X without a hex value" : "# without a decimal value"; break; }
            if (v > 255) { bad = "a # value above 255"; break; }
            out[k++] = (uint8_t)v;
        }
    }
    if (bad) { if (err && cap) lj_str_copy(err, cap, bad); return -1; }
    if (n) *n = k;
    if (err && cap) err[0] = 0;
    return 0;
}

/* ---- settings: Hardware.Ini [EXTAPP] (ours, additive; docs/file-formats.md) ---- */

int lj_extapp_ip_parse(const char *s, uint32_t *ip)
{
    uint32_t v = 0;
    int part, dots = 0;
    if (!s || !*s) return -1;
    for (;;) {
        int digits = 0;
        for (part = 0; *s >= '0' && *s <= '9' && digits < 3; s++, digits++) part = part * 10 + (*s - '0');
        if (!digits || part > 255) return -1;
        v = v << 8 | (uint32_t)part;
        if (*s == 0) break;
        if (*s != '.' || ++dots > 3) return -1;
        s++;
    }
    if (dots != 3) return -1;
    if (ip) *ip = v;
    return 0;
}

static int baud_ok(long b) { return b >= 110 && b <= 921600; }

static void link_check(lj_extapp_link *l)                          /* hostile ini: clamp a slot to something sendable or OFF */
{
    if (l->transport > LJ_EXTAPP_TCP) l->transport = LJ_EXTAPP_OFF;
    if (!baud_ok((long)l->baud)) l->baud = LJ_EXTAPP_BAUD_DEFAULT;
    if (l->transport == LJ_EXTAPP_SERIAL && !l->serial_port[0]) l->transport = LJ_EXTAPP_OFF;
    if (l->transport == LJ_EXTAPP_TCP && (!l->ip || !l->port)) l->transport = LJ_EXTAPP_OFF;
}

void lj_extapp_cfg_default(lj_extapp_cfg *c)
{
    int i;
    if (!c) return;
    memset(c, 0, sizeof *c);
    for (i = 0; i < LJ_SCENE_EXT; i++) c->link[i].baud = LJ_EXTAPP_BAUD_DEFAULT;
}

void lj_extapp_cfg_load(lj_ini *ini, lj_extapp_cfg *c)
{
    char k[24];
    int i, j;
    lj_extapp_cfg_default(c);
    if (!c || !ini) return;
    for (i = 0; i < LJ_SCENE_EXT; i++) {
        lj_extapp_link *l = &c->link[i];
        int f, t;
        long port;
        { if (snprintf(k, sizeof k, "Fixture%d", i + 1) < 0) k[0] = 0; }   f = lj_ini_geti(ini, "EXTAPP", k, 0);
        { if (snprintf(k, sizeof k, "Transport%d", i + 1) < 0) k[0] = 0; } t = lj_ini_geti(ini, "EXTAPP", k, 0);
        if (f < 1 || f > LJ_FIXTURES) continue;
        for (j = 0; j < i; j++) if (c->link[j].fixture == f) break;
        if (j < i) continue;                                       /* a fixture twice: the first slot wins */
        l->fixture = (uint8_t)f;
        l->transport = (uint8_t)(t >= 0 && t <= LJ_EXTAPP_TCP ? t : LJ_EXTAPP_OFF);
        { if (snprintf(k, sizeof k, "SerialPort%d", i + 1) < 0) k[0] = 0; } lj_str_copy(l->serial_port, sizeof l->serial_port, lj_ini_gets(ini, "EXTAPP", k, ""));
        { if (snprintf(k, sizeof k, "Baud%d", i + 1) < 0) k[0] = 0; }       l->baud = (uint32_t)lj_ini_geti(ini, "EXTAPP", k, LJ_EXTAPP_BAUD_DEFAULT);
        { if (snprintf(k, sizeof k, "Host%d", i + 1) < 0) k[0] = 0; }       if (lj_extapp_ip_parse(lj_ini_gets(ini, "EXTAPP", k, ""), &l->ip) != 0) l->ip = 0;
        { if (snprintf(k, sizeof k, "Port%d", i + 1) < 0) k[0] = 0; }       port = lj_ini_geti(ini, "EXTAPP", k, 0); l->port = (uint16_t)(port >= 1 && port <= 65535 ? port : 0);
        { if (snprintf(k, sizeof k, "Definition%d", i + 1) < 0) k[0] = 0; } if (lj_str_copy(l->def_file, sizeof l->def_file, lj_ini_gets(ini, "EXTAPP", k, "")) != 0) l->def_file[0] = 0;
        link_check(l);
    }
}

void lj_extapp_cfg_save(lj_ini *ini, const lj_extapp_cfg *c)
{
    char k[24], ip[16];
    int i;
    if (!ini || !c) return;
    for (i = 0; i < LJ_SCENE_EXT; i++) {
        const lj_extapp_link *l = &c->link[i];
        if (snprintf(ip, sizeof ip, "%u.%u.%u.%u", (unsigned)(l->ip >> 24), (unsigned)(l->ip >> 16 & 255u), (unsigned)(l->ip >> 8 & 255u), (unsigned)(l->ip & 255u)) < 0) ip[0] = 0;
        { if (snprintf(k, sizeof k, "Fixture%d", i + 1) < 0) k[0] = 0; }    lj_ini_seti(ini, "EXTAPP", k, l->fixture);
        { if (snprintf(k, sizeof k, "Transport%d", i + 1) < 0) k[0] = 0; }  lj_ini_seti(ini, "EXTAPP", k, l->transport);
        { if (snprintf(k, sizeof k, "SerialPort%d", i + 1) < 0) k[0] = 0; } lj_ini_sets(ini, "EXTAPP", k, l->serial_port);
        { if (snprintf(k, sizeof k, "Baud%d", i + 1) < 0) k[0] = 0; }       lj_ini_seti(ini, "EXTAPP", k, (int)l->baud);
        { if (snprintf(k, sizeof k, "Host%d", i + 1) < 0) k[0] = 0; }       lj_ini_sets(ini, "EXTAPP", k, l->ip ? ip : "");
        { if (snprintf(k, sizeof k, "Port%d", i + 1) < 0) k[0] = 0; }       lj_ini_seti(ini, "EXTAPP", k, l->port);
        { if (snprintf(k, sizeof k, "Definition%d", i + 1) < 0) k[0] = 0; } lj_ini_sets(ini, "EXTAPP", k, l->def_file);
    }
}

int lj_extapp_slot_of(const lj_extapp_cfg *c, int fixture)
{
    int i;
    for (i = 0; c && fixture >= 1 && i < LJ_SCENE_EXT; i++) if (c->link[i].fixture == fixture) return i;
    return -1;
}

lj_ini *lj_extapp_def_open(const char *path, char *err, size_t cap)
{
    FILE *f;
    long size;
    lj_ini *ini;
    const char *bad = NULL;
    if (!path || !*path) bad = "no definition file";
    else if ((f = fopen(path, "rb")) == NULL) bad = "cannot open the definition file";
    else {
        size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
        fclose(f);
        if (size < 0 || size > LJ_EXTAPP_DEF_MAX) bad = "the definition file is larger than 16 KB";
    }
    if (bad) { if (err && cap) lj_str_copy(err, cap, bad); return NULL; }
    if ((ini = lj_ini_load(path)) == NULL && err && cap) lj_str_copy(err, cap, "cannot read the definition file");
    return ini;
}

int lj_extapp_def_link(lj_ini *def, lj_extapp_link *out, char *err, size_t cap)
{
    const char *t = lj_ini_gets(def, "LJ4EVR", "TRANSPORT", ""), *bad = NULL;
    long v;
    out->transport = LJ_EXTAPP_OFF;
    if (!strcmp(t, "SERIAL")) {
        if (lj_str_copy(out->serial_port, sizeof out->serial_port, lj_ini_gets(def, "LJ4EVR", "SERIALPORT", "")) != 0 || !out->serial_port[0]) bad = "SERIALPORT missing or longer than 63 characters";
        else if (!baud_ok(v = lj_ini_geti(def, "LJ4EVR", "BAUD", LJ_EXTAPP_BAUD_DEFAULT))) bad = "BAUD outside 110..921600";
        else { out->baud = (uint32_t)v; out->transport = LJ_EXTAPP_SERIAL; }
    } else if (!strcmp(t, "TCP")) {
        if (lj_extapp_ip_parse(lj_ini_gets(def, "LJ4EVR", "HOST", ""), &out->ip) != 0) bad = "HOST is not a dotted IPv4 address";
        else if ((v = lj_ini_geti(def, "LJ4EVR", "PORT", 0)) < 1 || v > 65535) bad = "PORT outside 1..65535";
        else { out->port = (uint16_t)v; out->transport = LJ_EXTAPP_TCP; }
    } else bad = "no [LJ4EVR] TRANSPORT=SERIAL or TCP: nothing is sent";
    if (bad) { if (err && cap) lj_str_copy(err, cap, bad); return -1; }
    if (err && cap) err[0] = 0;
    return 0;
}

/* ---- the transports ---- */

_Static_assert(LJ_SCENE_EXT == 4, "the socket initialiser below names four slots");

static struct {
    lj_extapp_cfg cfg;
    struct {
        lj_serial *ser; lj_socket sock; uint8_t connecting, down, pending;
        uint64_t next_try_us, since_us;
        char last[LJ_EXT_CMD_CAP]; uint8_t have_last;              /* the last string seen (the change test), raw: "TRIG!1" != "TRIG!2" */
        uint8_t bytes[LJ_EXTAPP_MAX_BYTES]; size_t n;              /* encoded, waiting for the link */
    } s[LJ_SCENE_EXT];
    lj_extapp_stats st;
} ea = { .s = { { .sock = LJ_SOCKET_BAD }, { .sock = LJ_SOCKET_BAD }, { .sock = LJ_SOCKET_BAD }, { .sock = LJ_SOCKET_BAD } } };

static void link_close(int i)
{
    lj_serial_close(ea.s[i].ser); ea.s[i].ser = NULL;
    lj_socket_close(ea.s[i].sock); ea.s[i].sock = LJ_SOCKET_BAD;
    ea.s[i].connecting = 0; ea.st.connected[i] = 0;
}

static void link_fail(int i, uint64_t now, const char *what)       /* counted every time, logged once per outage; retry after LJ_EXTAPP_RECONNECT_US */
{
    ea.st.failed++;
    if (!ea.s[i].down) lj_log("extapp: fixture %u: %s (retry every %u ms)", (unsigned)ea.cfg.link[i].fixture, what, (unsigned)(LJ_EXTAPP_RECONNECT_US / 1000u));
    ea.s[i].down = 1;
    link_close(i);
    ea.s[i].next_try_us = now + LJ_EXTAPP_RECONNECT_US;
}

static int link_ready(int i, uint64_t now)                          /* 1 = bytes can go now */
{
    const lj_extapp_link *l = &ea.cfg.link[i];
    if (ea.s[i].ser || (ea.s[i].sock != LJ_SOCKET_BAD && !ea.s[i].connecting)) return 1;
    if (ea.s[i].connecting) {
        int r = lj_socket_connected(ea.s[i].sock);
        if (r < 0 || (r == 0 && now - ea.s[i].since_us > LJ_EXTAPP_RECONNECT_US)) { link_fail(i, now, "TCP connect failed"); return 0; }
        if (r == 0) return 0;
        ea.s[i].connecting = 0;
    } else {
        if (now < ea.s[i].next_try_us) return 0;
        if (l->transport == LJ_EXTAPP_SERIAL) {
            if ((ea.s[i].ser = lj_serial_open(l->serial_port, l->baud)) == NULL) { link_fail(i, now, "serial port did not open"); return 0; }
            lj_log("extapp: fixture %u: serial %s %u baud open", (unsigned)l->fixture, l->serial_port, (unsigned)l->baud);
        } else {
            if ((ea.s[i].sock = lj_socket_connect(l->ip, l->port)) == LJ_SOCKET_BAD) { link_fail(i, now, "TCP connect failed"); return 0; }
            ea.s[i].connecting = 1; ea.s[i].since_us = now;
            return link_ready(i, now);
        }
    }
    if (l->transport == LJ_EXTAPP_TCP)
        lj_log("extapp: fixture %u: TCP client to %u.%u.%u.%u:%u connected", (unsigned)l->fixture, (unsigned)(l->ip >> 24), (unsigned)(l->ip >> 16 & 255u), (unsigned)(l->ip >> 8 & 255u), (unsigned)(l->ip & 255u), (unsigned)l->port);
    ea.s[i].down = 0; ea.st.connected[i] = 1;
    return 1;
}

int lj_extapp_open(const lj_extapp_cfg *c)
{
    int i;
    lj_extapp_close();
    if (c) ea.cfg = *c;
    for (i = 0; i < LJ_SCENE_EXT; i++) link_check(&ea.cfg.link[i]);   /* opens nothing: a link connects when its first string is due (W5-X-OFF) */
    return 0;
}

void lj_extapp_close(void)
{
    int i;
    for (i = 0; i < LJ_SCENE_EXT; i++) {
        link_close(i);
        ea.s[i].pending = ea.s[i].have_last = ea.s[i].down = 0; ea.s[i].next_try_us = 0;
    }
}

void lj_extapp_step(const uint8_t ext_fixture[LJ_SCENE_EXT], const char *ext, uint64_t now_us)
{
    int j, i;
    for (j = 0; ext_fixture && ext && j < LJ_SCENE_EXT; j++) {
        const char *cmd = ext + (size_t)j * LJ_EXT_CMD_CAP;
        char err[64];
        if (!ext_fixture[j] || (i = lj_extapp_slot_of(&ea.cfg, ext_fixture[j])) < 0 || ea.cfg.link[i].transport == LJ_EXTAPP_OFF) continue;
        if (!memchr(cmd, 0, LJ_EXT_CMD_CAP) || (ea.s[i].have_last && !strcmp(ea.s[i].last, cmd))) continue;   /* sent only on change */
        memcpy(ea.s[i].last, cmd, strlen(cmd) + 1); ea.s[i].have_last = 1;
        if (lj_extapp_encode(cmd, ea.s[i].bytes, &ea.s[i].n, err, sizeof err) != 0) { ea.st.refused++; ea.s[i].pending = 0; lj_log("extapp: fixture %u: \"%s\" refused: %s", (unsigned)ext_fixture[j], cmd, err); continue; }
        ea.s[i].pending = ea.s[i].n > 0;                             /* the latest string replaces one still waiting for its link */
    }
    for (i = 0; i < LJ_SCENE_EXT; i++) {
        uint8_t drain[256];
        int r = 0;
        if (ea.s[i].sock != LJ_SOCKET_BAD && !ea.s[i].connecting && (r = lj_socket_recv(ea.s[i].sock, drain, sizeof drain)) < 0) link_fail(i, now_us, "TCP peer closed");   /* replies are ignored, a close is seen */
        if (!ea.s[i].pending || !link_ready(i, now_us)) continue;
        r = ea.s[i].ser ? lj_serial_write(ea.s[i].ser, ea.s[i].bytes, ea.s[i].n) : lj_socket_send(ea.s[i].sock, ea.s[i].bytes, ea.s[i].n);
        if (r == (int)ea.s[i].n) { ea.st.sent++; ea.s[i].pending = 0; }
        else if (r != 0) link_fail(i, now_us, ea.s[i].ser ? "serial write failed" : "TCP send failed");   /* ponytail: a partial write counts as a failure and resends whole on reconnect */
    }
}

void lj_extapp_stats_get(lj_extapp_stats *out) { if (out) *out = ea.st; }
