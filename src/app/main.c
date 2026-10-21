/* main.c - lj4evr entry point: setlocale once, then lj_app_main (design 2.9). SDL_main.h supplies WinMain for the WIN32 subsystem. */
#include <SDL3/SDL_main.h>
#include <locale.h>
#include "app/lj_app.h"

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C");   /* strings are CP1252 bytes (design 2.1); no locale-dependent ctype anywhere */
    return lj_app_main(argc, argv);
}
