
#include <stdio.h>
#include <stdlib.h>

#include "webtpl.h"

main(int argc, char **argv)
{
  char *tpl;
  char *a;
  char **a2;
  char **z;
  int ret;

  fprintf(stderr, "webtpl test.\nlibrary version: %s\n", webtpl_version);

  WebTemplate W = newWebTemplate();

  WebTemplate_add_header(W, "Cache-Control",
                "no-store, no-cache, must-revalidate");
  WebTemplate_add_header(W, "Expires",
                "Sat, 1 Jan 2000 01:01:01 GMT");

  WebTemplate_set_comments(W, "NOTE", "ENDNOTE");
  
  ret = WebTemplate_get_by_name(W, "page", "not-there.tpl");
  printf("Expected missing file: (%d), %s\n", ret,  WebTemplate_get_error_string(W));
  fflush(stdout);
  ret = WebTemplate_get_by_name(W, "page", "bad-tpl.tpl");
  printf("Expected invalid file: (%d), %s\n", ret,  WebTemplate_get_error_string(W));
  fflush(stdout);

  WebTemplate_get_by_name(W, "page", "test1.tpl");

  WebTemplate_set_comments(W, "#", NULL);
  WebTemplate_get_by_name(W, "sub", "test2.tpl");
  WebTemplate_get_by_name(W, "sub3", "test3.tpl");

  // open a second
  WebTemplate WW = newWebTemplate();

  /* args */
  WebTemplate_get_args(W);
  a = WebTemplate_get_arg(W, "arg1");
  if (a) {
    WebTemplate_assign(W, "A1", a);
    free(a);
  }
  a = WebTemplate_get_arg(W, "arg3");
  if (a) {
    WebTemplate_assign(W, "A3", a);
    free(a);
  }
  a = WebTemplate_get_arg(WW, "arg3");
  if (a) {
    WebTemplate_assign(WW, "A3", a);
    free(a);
  }
  a2 = WebTemplate_get_arg_list(W, "arg2");
  for (z=a2;z&&*z;z++) {
     WebTemplate_assign(W, "A2", *z);
     WebTemplate_parse_dynamic(W, "page.argv");
  }
  WebTemplate_free_arg_list(a2);
  
  WebTemplate_free(WW);

  WebTemplate_assign(W, "AA", "aa");
  WebTemplate_assign(W, "CC", "cc");
  WebTemplate_parse(W, "SUB", "sub");

  WebTemplate_parse_dynamic(W, "sub3.dyn3");
  WebTemplate_parse(W, "SUB3", "sub3");
 
  WebTemplate_assign(W, "REPL", "replacement");
  WebTemplate_assign(W, "EFGH", "999");

  WebTemplate_assign(W, "ABCD", "(111)");
  WebTemplate_parse_dynamic(W, "page.zzzz");
  WebTemplate_assign(W, "ABCD", "(222)");
  WebTemplate_parse_dynamic(W, "page.zzzz");

  WebTemplate_assign(W, "_EFG", "(-efg-)");
  WebTemplate_parse_dynamic(W, "page.abc_d.efg");

  WebTemplate_parse_dynamic(W, "page.abc_d");

  WebTemplate_parse_dynamic(W, "page.abc_d");
  
  WebTemplate_parse(W, "PAGE", "page");

  WebTemplate_set_cookie(W, "ck1", "ck1's value",
      0, NULL, "/fox/", 0);
  WebTemplate_write(W, "PAGE");

  /* try the reset/free stuff */

  WebTemplate_reset_output(W);

  char *v = WebTemplate_macro_value(W, "TPLMAC");
  WebTemplate_assign(W, "AA", v);
  free(v);
  v = WebTemplate_text2html("<br>&heloo CC <br>");
  WebTemplate_assign(W, "CC", v);
  free(v);
  WebTemplate_set_cookie(W, "ck2", "ck2's value",
      0, NULL, "/fox/", 0);
  WebTemplate_parse(W, "PAGE", "page");
  WebTemplate_write(W, "PAGE");

  /* Send a text only page */

  WebTemplate_reset_output(W);
  WebTemplate_get_by_name(W, "txt", "test4.tpl");
  WebTemplate_assign(W, "TEXT", "Text inserted,\nby program.\n");
  WebTemplate_add_header(W, "Content-type", "text/plain");
  
  WebTemplate_parse(W, "PAGE", "txt");
  WebTemplate_write(W, "PAGE");
  
  /* for no good reason, free the template */
  WebTemplate_free(W);

}

