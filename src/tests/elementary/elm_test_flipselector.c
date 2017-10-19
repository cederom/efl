#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_ACCESS_PROTECTED
#include <Elementary.h>
#include "elm_suite.h"


START_TEST (elm_atspi_role_get)
{
   Evas_Object *win, *flipselector;
   Efl_Access_Role role;

   elm_init(1, NULL);
   win = elm_win_add(NULL, "flipselector", ELM_WIN_BASIC);

   flipselector = elm_flipselector_add(win);
   role = efl_access_role_get(flipselector);

   ck_assert(role == EFL_ACCESS_ROLE_LIST);

   elm_shutdown();
}
END_TEST

void elm_test_flipselector(TCase *tc)
{
 tcase_add_test(tc, elm_atspi_role_get);
}
