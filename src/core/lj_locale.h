/* lj_locale.h - the host locale's decimal separator (decision D6: percentages in
 * the fixture controls follow the Windows locale, "Intensity 100,0%" on Martin's
 * Danish machines). Reads localeconv(), so app/main.c must have called
 * setlocale(LC_NUMERIC, "") once (LORD_C-19); without it the answer is '.'. */
#ifndef LJ_CORE_LOCALE_H
#define LJ_CORE_LOCALE_H

char lj_locale_decimal(void);      /* ',' or '.' (anything else the C library reports collapses to '.') */

#endif /* LJ_CORE_LOCALE_H */
