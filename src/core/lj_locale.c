/* lj_locale.c - decimal separator from the C locale (D6). */
#include "core/lj_locale.h"

#include <locale.h>

char lj_locale_decimal(void)
{
    const struct lconv *lc = localeconv();
    return (lc && lc->decimal_point && lc->decimal_point[0] == ',') ? ',' : '.';
}
