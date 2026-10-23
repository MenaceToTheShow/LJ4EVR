#!/usr/bin/env python3
"""test_layering.py - the mechanical conventions of design section 2 / 2.0, run by CTest.

Checks, over src/:
  1. The allowed-includes graph: an #include "<dir>/..." may only point at a layer
     the including layer sits above; <SDL3/...> only under platform/ and app/.
  2. OS headers only on the per-file allowlist (platform/: anything).
  3. No float, double, <math.h>, <stdatomic.h> or volatile in any SDL-free directory
     (comments and string literals stripped first).
  4. No lj_table_* call and no cue-list symbol (lj_cl_*, lj_cuelist*) inside engine/ (ME-121: the cue list runs
     on the UI thread, design 2.3 rule 4).
  5. File names lj_<name>.c/.h, include guards LJ_<DIR>_<NAME>_H.
  6. Every header compiles alone in its own translation unit (compiler from --cc).
  7. ME-101 counts, both must stay 0: byte-identical do-nothing widget handlers (lj_handler_noop is the one) and
     "x ? lj_ini_get*" ternaries (lj_ini_geti / lj_ini_gets read a NULL ini as empty).
  8. ME-44: every .c has its .h (same stem), unless a blessed group header declares it (GROUP_HEADERS) or it is
     on PAIR_EXEMPT; every identifier a header declares at file scope (macros, enum constants, tags, typedefs,
     functions, variables) starts with lj_ / LJ_; every OS_ALLOW row names a file that has that include.

usage: test_layering.py --src <repo>/src --cc <compiler> [--gen <dir with version.h>]
exit 0 = clean, 1 = findings printed one per line.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

LAYERS = {
    "core":     {"core"},
    "fixlib":   {"core", "fixlib"},
    "model":    {"core", "fixlib", "model"},
    "engine":   {"core", "fixlib", "model", "engine"},
    "io":       {"core", "fixlib", "model", "io"},
    "gfx":      {"core", "gfx"},
    "ui":       {"core", "gfx", "ui"},
    "desktop":  {"core", "gfx", "ui", "model", "engine", "fixlib", "io", "desktop", "controls", "windows"},
    "controls": {"core", "gfx", "ui", "model", "engine", "fixlib", "io", "desktop", "controls", "windows"},
    "windows":  {"core", "gfx", "ui", "model", "engine", "fixlib", "io", "desktop", "controls", "windows"},
    "platform": {"core", "gfx", "ui", "engine", "io", "platform"},
    "app":      {"core", "gfx", "ui", "model", "engine", "fixlib", "io", "desktop", "controls", "windows", "platform", "app"},
}
SDL_ALLOWED = {"platform", "app"}
SDL_FREE = {"core", "model", "engine", "fixlib", "io", "gfx", "ui", "desktop", "controls", "windows"}

OS_HEADERS = {"windows.h", "unistd.h", "pthread.h", "winsock2.h", "ws2tcpip.h", "iphlpapi.h", "mmsystem.h",
              "ifaddrs.h", "net/if.h", "netinet/in.h", "arpa/inet.h", "sys/socket.h", "CoreMIDI/CoreMIDI.h",
              "direct.h", "io.h", "dirent.h"}   # ME-113 / ME-44: the CRT directory headers too
OS_ALLOW = {   # relative to src/, forward slashes
    "core/lj_clock.c": {"windows.h", "time.h"},
    "core/lj_lock.c": {"windows.h", "pthread.h"},
    "io/lj_net.c": {"winsock2.h", "ws2tcpip.h", "iphlpapi.h", "sys/socket.h", "ifaddrs.h", "net/if.h", "netinet/in.h", "arpa/inet.h", "unistd.h"},
    "io/lj_sacn.c": {"winsock2.h", "ws2tcpip.h", "sys/socket.h", "netinet/in.h", "arpa/inet.h", "unistd.h"},   # ME-44: iphlpapi.h / ifaddrs.h live in lj_net.c
    "io/lj_midi_winmm.c": {"windows.h", "mmsystem.h"},
    "model/lj_library.c": {"sys/stat.h", "direct.h", "io.h", "dirent.h"},   # the library folder walk and rmdir (_findfirst / opendir)
    "model/lj_movemacro.c": {"io.h", "dirent.h"},   # W3: the MacroShapes\*.bez listing
    "io/lj_midi_coremidi.c": {"CoreMIDI/CoreMIDI.h"},
    "app/lj_sysmem.c": {"windows.h", "unistd.h", "sys/sysctl.h"},   # W2 app: the memory gauge free-memory query (ME-87)
    "core/lj_fsinfo.c": {"sys/types.h", "sys/stat.h", "direct.h"},   # W3 movement: file mtime (LORD_FIDELITY-12) and lj_fs_mkdir (_mkdir)
}
NAME_EXEMPT = {"app/main.c"}
# ME-44: the group headers blessed in docs/conventions.md and design 2.0. A ui/widgets/lj_w_<x>.c without its own
# header is fine when one of these declares lj_w_<x>*.
GROUP_HEADERS = ["ui/widgets/lj_widgets_basic.h", "ui/widgets/lj_widgets_lists.h"]
PAIR_EXEMPT = {   # .c files with no same-stem header, and why
    "app/main.c": "the program entry",
    "app/lj_render_main.c": "the lj4evr_render entry",
    "gfx/lj_art_gen.c": "generated; declared by gfx/lj_glyphs.h",
    "gfx/lj_fonts_gen.c": "generated; declared by gfx/lj_font.h",
    "io/lj_midi_winmm.c": "a backend of io/lj_midi.h",
    "io/lj_midi_coremidi.c": "a backend of io/lj_midi.h",
    "io/lj_midi_null.c": "a backend of io/lj_midi.h",
}
ID = re.compile(r"[A-Za-z_]\w*")
ID_OK = re.compile(r"^(lj_|LJ_)")
KEYWORDS = {"void", "int", "char", "unsigned", "signed", "short", "long", "const", "static", "extern", "struct", "enum",
            "union", "typedef", "inline", "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t",
            "int32_t", "int64_t", "bool", "_Bool", "restrict", "_Static_assert", "register", "auto"}

NOOP_RE = re.compile(r'void\s+(\w+)\s*\(\s*lj_widget\s*\*\s*w\s*,\s*void\s*\*\s*ud\s*\)\s*\{\s*\(void\)\s*w\s*;\s*\(void\)\s*ud\s*;\s*\}')
SCENE_GETTER_RE = re.compile(r'\bstatic\s+(?:const\s+)?lj_scene\s*\*\s*(\w+)\s*\(\s*void\s*\)')                       # ME-129
COPY_RE = re.compile(r'\bstatic\s+(?:(?:int|void)\s+(\w+)\s*\(\s*lj_window\s*\*\s*\w+\s*,\s*const\s+char\s*\*\s*name\s*,\s*const\s+char\s*\*\s*\w+\s*\)'
                     r'|lj_library\s*\*\s*(lib)\s*\(\s*void\s*\)|lj_ctx\s*\*\s*(ctx)\s*\(\s*void\s*\))')                    # ME-129
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)


def strip_code(text, strings=True):
    """Remove comments (and, with strings, string/char literals) so the greps see code only."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif strings and c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out)


def drop_pp(code):
    out, cont = [], False
    for line in code.split(chr(10)):
        pp = cont or line.lstrip().startswith("#")
        cont = pp and line.rstrip().endswith(chr(92))
        out.append("" if pp else line)
    return chr(10).join(out)


def header_ids(text):
    """(kind, name) of every file-scope identifier a header declares or defines."""
    code = strip_code(text, strings=True)
    out = []
    for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", code, re.M):
        out.append(("macro", m.group(1)))
    code = drop_pp(code)
    # walk at depth 0; collect enum bodies
    depth, buf, stmts, i = 0, [], [], 0
    while i < len(code):
        c = code[i]
        if c == "{":
            if depth == 0:
                head = "".join(buf)
                j, d = i, 0
                while True:
                    if code[j] == "{": d += 1
                    elif code[j] == "}":
                        d -= 1
                        if d == 0: break
                    j += 1
                body = code[i + 1:j]
                if re.search(r"\benum\b[^;]*$", head):
                    for item in body.split(","):
                        m = ID.match(item.strip())
                        if m: out.append(("enum constant", m.group(0)))
                buf.append(" {} ")
                i = j + 1
                continue
        if c == ";" and depth == 0:
            stmts.append("".join(buf)); buf = []
        else:
            buf.append(c)
        i += 1
    for s in stmts:
        s = " ".join(s.split())
        if not s or s.startswith("_Static_assert"): continue
        for m in re.finditer(r"\b(struct|enum|union)\s+(\w+)\s*\{", s):
            out.append((m.group(1) + " tag", m.group(2)))
        if s.startswith("typedef"):
            m = re.search(r"\(\s*\*\s*(\w+)\s*\)", s)
            name = m.group(1) if m else ID.findall(re.sub(r"\[.*?\]", "", s))[-1]
            out.append(("typedef", name))
            continue
        if "(" in s:
            m = re.search(r"(\w+)\s*$", s[:s.index("(")])
            if m and m.group(1) not in KEYWORDS:
                out.append(("function", m.group(1)))
            else:
                m = re.search(r"\(\s*\*\s*(\w+)\s*\)", s)
                if m: out.append(("variable", m.group(1)))
            continue
        decl = re.sub(r"\{\s*\}", "", s)
        for part in decl.split(","):
            part = re.sub(r"=.*", "", re.sub(r"\[.*?\]", "", part)).strip()
            ids = ID.findall(part)
            if ids and ids[-1] not in KEYWORDS and not re.fullmatch(r"(struct|enum|union)\s+\w+", part):
                out.append(("variable", ids[-1]))
    return out


def selfcheck_ids():
    """The identifier scan must see every kind it claims to (a planted header)."""
    planted = ("#define BAR 1\nint foo(void);\nvoid lj_ok(void (*cb)(int x), void *ud);\nenum lj_e { LJ_A, BAD_K = 2 };\n"
               "typedef struct { int a; } bad_t;\ntypedef void (*bad_fn)(void);\nextern const int bad_var[3], lj_v;\n")
    names = sorted(n for _, n in header_ids(planted) if not ID_OK.match(n))
    assert names == ["BAD_K", "BAR", "bad_fn", "bad_t", "bad_var", "foo"], names


def layer_of(rel):
    return rel.split("/", 1)[0]


def compile_alone(cc, src_dir, gen_dir, header, tmp):
    tu = os.path.join(tmp, "tu_" + os.path.basename(header).replace(".", "_") + ".c")
    with open(tu, "w", encoding="ascii") as f:
        f.write('#include "%s"\nint lj_tu_dummy(void);\nint lj_tu_dummy(void) { return 0; }\n' % header)
    base = os.path.basename(cc).lower()
    inc = [src_dir] + ([gen_dir] if gen_dir else [])
    if base in ("cl", "cl.exe"):
        cmd = [cc, "/nologo", "/c", "/std:c11", "/W4", "/WX", "/permissive-", "/utf-8", "/Fo" + tu + ".obj"]
        cmd += ["/I" + d for d in inc] + [tu]
        env = os.environ.copy()
        if "INCLUDE" not in env:          # cl outside a vcvars shell: route through vcvars64.bat
            vc = os.path.abspath(cc)
            while vc and os.path.basename(vc) != "VC":
                parent = os.path.dirname(vc)
                if parent == vc:
                    vc = ""
                    break
                vc = parent
            bat = os.path.join(vc, "Auxiliary", "Build", "vcvars64.bat") if vc else ""
            if not os.path.isfile(bat):
                return "cannot find vcvars64.bat for %s" % cc
            quoted = " ".join('"%s"' % a for a in cmd)
            cmd = ["cmd", "/c", '"%s" >nul 2>nul && %s' % (bat, quoted)]
            cmd = " ".join(cmd[:2]) + ' "' + cmd[2] + '"'
        p = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=tmp)
    else:
        cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Werror", "-fsyntax-only"]
        cmd += ["-I" + d for d in inc] + [tu]
        p = subprocess.run(cmd, capture_output=True, text=True, cwd=tmp)
    if p.returncode != 0:
        return (p.stdout + p.stderr).strip()
    return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--cc", required=True)
    ap.add_argument("--gen", default="")
    a = ap.parse_args()
    src = os.path.abspath(a.src)
    findings = []
    headers = []
    nfiles = 0
    noops = ternaries = scene_getters = helper_copies = 0
    selfcheck_ids()
    group_text = "".join(open(os.path.join(src, g), encoding="latin-1").read() for g in GROUP_HEADERS)
    for g in GROUP_HEADERS:
        if not os.path.exists(os.path.join(src, g)):
            findings.append("%s: blessed group header missing" % g)
    for rel, hdrs in OS_ALLOW.items():          # ME-44: no phantom allow rows
        p = os.path.join(src, rel)
        incs = set(i for _, i in INCLUDE_RE.findall(strip_code(open(p, encoding="latin-1").read(), strings=False))) if os.path.exists(p) else set()
        for h in sorted(hdrs - incs):
            findings.append("%s: OS_ALLOW row for <%s> but the file does not include it" % (rel, h))
    for dirpath, _, files in os.walk(src):
        for fn in sorted(files):
            if not fn.endswith((".c", ".h")):
                continue
            path = os.path.join(dirpath, fn)
            rel = os.path.relpath(path, src).replace("\\", "/")
            layer = layer_of(rel)
            nfiles += 1
            if layer not in LAYERS:
                findings.append("%s: unknown layer directory" % rel)
                continue
            text = open(path, encoding="latin-1").read()
            code = strip_code(text)                           # for the word greps
            nocomment = strip_code(text, strings=False)       # for #include lines (the file name is a string literal)
            # 8. ME-44 pairing and header identifiers
            if fn.endswith(".c") and rel not in PAIR_EXEMPT and not os.path.exists(path[:-2] + ".h"):
                stem = fn[:-2]
                if not (rel.startswith("ui/widgets/lj_w_") and re.search(r"\b%s\w*" % stem, group_text)):
                    findings.append("%s: no %s.h and no blessed group header declares it (ME-44)" % (rel, stem))
            if fn.endswith(".h"):
                for kind, name in header_ids(text):
                    if not ID_OK.match(name):
                        findings.append("%s: %s '%s' does not start with lj_/LJ_ (ME-44)" % (rel, kind, name))
            # 5. names and guards
            if rel not in NAME_EXEMPT and not fn.startswith("lj_"):
                findings.append("%s: file name must be lj_<name>.c/.h" % rel)
            if fn.endswith(".h"):
                headers.append(rel)
                parts = rel[:-2].split("/")                      # ui/forms/lj_form -> LJ_UI_FORMS_FORM_H
                stem = parts[-1][3:] if parts[-1].startswith("lj_") else parts[-1]
                guard = "LJ_" + "_".join(parts[:-1] + [stem]).upper() + "_H"
                m = re.search(r'#ifndef\s+(\w+)\s*\n\s*#define\s+\1\b', text)
                if not m:
                    findings.append("%s: no include guard" % rel)
                elif m.group(1) != guard:
                    findings.append("%s: include guard %s, expected %s" % (rel, m.group(1), guard))
            # 1 + 2. includes
            for kind, inc in INCLUDE_RE.findall(nocomment):
                if inc.startswith("SDL3/"):
                    if layer not in SDL_ALLOWED:
                        findings.append("%s: <%s> outside platform/ and app/" % (rel, inc))
                    continue
                if kind == '"' and "/" in inc and inc.split("/", 1)[0] in LAYERS:
                    target = inc.split("/", 1)[0]
                    if target not in LAYERS[layer]:
                        findings.append("%s: includes %s across the layer graph (%s may include %s)"
                                        % (rel, inc, layer, ", ".join(sorted(LAYERS[layer]))))
                if inc in OS_HEADERS or inc.startswith("sys/"):
                    if layer != "platform" and inc not in OS_ALLOW.get(rel, set()):
                        findings.append("%s: OS header <%s> not on the allowlist" % (rel, inc))
                if inc == "math.h" and layer in SDL_FREE:
                    findings.append("%s: <math.h> in an SDL-free directory" % rel)
                if inc == "stdatomic.h" and layer in SDL_FREE:
                    findings.append("%s: <stdatomic.h> in an SDL-free directory" % rel)
            # 3. float/double/volatile
            if layer in SDL_FREE:
                for word in ("float", "double", "volatile"):
                    if re.search(r'\b%s\b' % word, code):
                        findings.append("%s: '%s' in an SDL-free directory" % (rel, word))
            # 4. engine never looks a record up
            if layer == "engine" and re.search(r'\blj_table_\w+\s*\(', code):
                findings.append("%s: lj_table_* call inside engine/" % rel)
            if layer == "engine" and re.search(r'\b(lj_cl_|lj_cuelist)\w*', code):
                findings.append("%s: cue-list symbol inside engine/ (the runner is UI-thread, design 2.3 rule 4)" % rel)
            # 7. ME-101 counts
            for m in NOOP_RE.finditer(text):
                if m.group(1) != "lj_handler_noop":
                    noops += 1
                    findings.append("%s: %s is a copy of lj_handler_noop (ME-101)" % (rel, m.group(1)))
            for _ in re.finditer(r'\?\s*lj_ini_get[is]\s*\(', code):
                ternaries += 1
                findings.append("%s: \"? lj_ini_get\" ternary; lj_ini_geti/gets take a NULL ini (ME-101)" % rel)
            # 9. ME-129 counts, both must stay 0: private current-scene getters (lj_ctx_scene is the one) and private
            #    set-caption-by-name / lib() / ctx() copies (lj_window_set_text, lj_ctx_get()->lib, lj_ctx_get())
            for m in SCENE_GETTER_RE.finditer(code):
                scene_getters += 1
                findings.append("%s: %s is a private current-scene getter; use lj_ctx_scene (ME-129)" % (rel, m.group(1)))
            for m in COPY_RE.finditer(code):
                helper_copies += 1
                findings.append("%s: %s copies lj_window_set_text / lj_ctx_get (ME-129)" % (rel, m.group(1) or m.group(2) or m.group(3)))
    # 6. headers alone
    with tempfile.TemporaryDirectory() as tmp:
        for h in headers:
            err = compile_alone(a.cc, src, a.gen, h, tmp)
            if err:
                findings.append("%s: does not compile alone:\n    %s" % (h, err.replace("\n", "\n    ")))
    for f in findings:
        print(f)
    print("test_layering: %d file(s) checked, %d header(s) compiled alone, noop handlers %d, ini ternaries %d, "
          "scene getters %d, set-text/lib/ctx copies %d, %d finding(s)"
          % (nfiles, len(headers), noops, ternaries, scene_getters, helper_copies, len(findings)))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
