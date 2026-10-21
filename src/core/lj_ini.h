/* lj_ini.h - Windows-style INI files (Main.Ini, forms.ini, Tabs.Ini), CP1252 bytes.
 * Design 2.1. Prototypes verbatim; lj_ini_new / lj_ini_sets / lj_ini_free added
 * because a loader that returns a pointer needs a constructor for a missing file,
 * a string setter and a destructor. */
#ifndef LJ_CORE_INI_H
#define LJ_CORE_INI_H

typedef struct lj_ini lj_ini;

lj_ini     *lj_ini_load(const char *path);                                        /* NULL when the file cannot be read */
lj_ini     *lj_ini_new(void);                                                     /* empty document */
int         lj_ini_save(lj_ini *ini, const char *path);                           /* 0 / -1 */
int         lj_ini_geti(lj_ini *ini, const char *sec, const char *key, int dflt);
void        lj_ini_seti(lj_ini *ini, const char *sec, const char *key, int v);
const char *lj_ini_gets(lj_ini *ini, const char *sec, const char *key, const char *dflt);  /* pointer valid until the next set/free on this ini (LORD_C-18) */
void        lj_ini_sets(lj_ini *ini, const char *sec, const char *key, const char *v);
void        lj_ini_free(lj_ini *ini);                                             /* NULL is a no-op */

#endif /* LJ_CORE_INI_H */
