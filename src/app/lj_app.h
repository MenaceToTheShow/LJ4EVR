/* lj_app.h - lj4evr startup and shutdown (design 2.9, [01 §6.2]), W0 scope:
 * Main.Ini from SDL_GetPrefPath, the stub main window, TSEQUENCEFORM and
 * TCURRENTCUE from the generated tables, the engine thread, the UI loop, and
 * forms.ini on close. Switches: --save <bmp> (main window surface), --selftest
 * [ref.bmp] (paint TSEQUENCEFORM on screen, read it back, compare with the
 * lj4evr_render image of the same form; prints OK/FAIL, exit 0/1), --explode
 * (offset covering panels, design 7.6), --ms <n> (quit after n ms, for
 * tools/shot.ps1), --native, --zoom <1|2> ([CHROME] overrides). */
#ifndef LJ_APP_APP_H
#define LJ_APP_APP_H

int lj_app_main(int argc, char **argv);   /* the whole program; returns the process exit code */

#endif /* LJ_APP_APP_H */
