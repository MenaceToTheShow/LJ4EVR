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
    "io/lj_sacn.c": {"winsock2.h", "ws2tcpip.h", "iphlpapi.h", "sys/socket.h", "ifaddrs.h", "netinet/in.h", "arpa/inet.h", "unistd.h"},
    "io/lj_midi_winmm.c": {"windows.h", "mmsystem.h"},
    "model/lj_library.c": {"sys/stat.h", "direct.h", "io.h", "dirent.h"},   # the library folder walk and rmdir (_findfirst / opendir)
    "model/lj_movemacro.c": {"io.h", "dirent.h"},   # W3: the MacroShapes\*.bez listing
    "io/lj_midi_coremidi.c": {"CoreMIDI/CoreMIDI.h"},
    "app/lj_sysmem.c": {"windows.h", "unistd.h", "sys/sysctl.h"},   # W2 app: the memory gauge free-memory query (ME-87)
    "core/lj_fsinfo.c": {"sys/types.h", "sys/stat.h", "direct.h"},   # W3 movement: file mtime (LORD_FIDELITY-12) and lj_fs_mkdir (_mkdir)
}
NAME_EXEMPT = {"app/main.c"}

NOOP_RE = re.compile(r'void\s+(\w+)\s*\(\s*lj_widget\s*\*\s*w\s*,\s*void\s*\*\s*ud\s*\)\s*\{\s*\(void\)\s*w\s*;\s*\(void\)\s*ud\s*;\s*\}')
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
    noops = ternaries = 0
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
    # 6. headers alone
    with tempfile.TemporaryDirectory() as tmp:
        for h in headers:
            err = compile_alone(a.cc, src, a.gen, h, tmp)
            if err:
                findings.append("%s: does not compile alone:\n    %s" % (h, err.replace("\n", "\n    ")))
    for f in findings:
        print(f)
    print("test_layering: %d file(s) checked, %d header(s) compiled alone, noop handlers %d, ini ternaries %d, %d finding(s)"
          % (nfiles, len(headers), noops, ternaries, len(findings)))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
