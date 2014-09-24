/* ========================================================================
 * Copyright (c) 2004-2008 The University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ========================================================================
 */


/* (webtpl) web template package definitions */

#ifndef webtpl_h
#define webtpl_h

#ifdef LIBRARY
/* Template macro definition */

#define TM_TEXT   1
#define TM_TMPL   2

typedef struct TmplMacro__ {
  struct TmplMacro__ *next;
  char *name;
  char *value;
  size_t len;
  char *xtra1;
  char *xtra2;
} TmplMacro_, *TmplMacro;

/* Template item */

#define TI_TEXT    1
#define TI_MACRO   2
#define TI_DYNAMIC 3
#define TI_DTEXT   4

typedef struct TmplItem__ {
  struct TmplItem__ *next;
  void  *parent;            /* parent template */
  int    type;
  void  *content;           /* text, macro name, dynamic block template */
  size_t len;               /* length of text item */
} TmplItem_, *TmplItem;

/* Template */

typedef struct Template__ {
  struct Template__ *next;     /* next template ( if plain ) */
  void *base;
  char *name;
  TmplItem pip;             /* parent item pointer ( if dynamic ) */
  TmplItem item;            /* template items */
  TmplItem last;            /* end of item list */
} Template_, *Template;
#define MACROS(T) (((TmplBase)T->base)->macros)


typedef struct WebTemplate__ {
  Template template;
  TmplMacro macros;
  TmplMacro arg;            /* form and url args (decoded) */
  TmplMacro in_cookie;      /* cookies (incoming) */
  TmplMacro header;         /* headers (outgoing) */
  TmplMacro octet;          /* octet data (incoming) */
  int header_sent;
  int fd;                   /* usually just stdout */
  int cip;                  /* 'comments' in-progress */
  char *cstart;             /* text to signal start-of-comment */
  size_t lcstart;
  char *cend;               /* text to signal end-of-comment */
  size_t lcend;
  char *error_string;       /* text of error */
} WebTemplate_, *WebTemplate;
  
#else /* LIBRARY */

#include <stdio.h>
#include <time.h>

typedef void *WebTemplate;
WebTemplate WebTemplate_new();
WebTemplate newWebTemplate();
void WebTemplate_free();
void freeWebTemplate();
char *WebTemplate_macro_value(WebTemplate, char *);
int WebTemplate_get_by_fd(WebTemplate W, char *name, int fd);
int WebTemplate_get_by_fp(WebTemplate W, char *name, FILE *f);
int WebTemplate_get_by_name(WebTemplate W, char *name, char *filename);
void WebTemplate_assign(WebTemplate W, char *name, char *value);
void WebTemplate_assign_int(WebTemplate W, char *name, int value);
int WebTemplate_parse_dynamic(WebTemplate W, char *dname);
int WebTemplate_parse(WebTemplate W, char *mname, char *tname);

void WebTemplate_add_header(WebTemplate W, char *name, char *value);
void WebTemplate_set_cookie(WebTemplate W, char *name, char *argvalue,
        time_t argexp, char *argdomain, char *argpath, int secure);
void WebTemplate_get_args(WebTemplate W);
char *WebTemplate_get_arg(WebTemplate W, char *name);
char **WebTemplate_get_arg_list(WebTemplate W, char *name);
void WebTemplate_free_arg_list(char **list);
int WebTemplate_get_octet_arg(WebTemplate W, char *name,
     void **value, int *len, char **type, char **filename);
char *WebTemplate_get_next_arg(WebTemplate W, int *n, char **v);
char *WebTemplate_get_cookie(WebTemplate W, char *name);
void WebTemplate_set_output(WebTemplate W, int fd);
void WebTemplate_set_noheader(WebTemplate W);
int WebTemplate_header(WebTemplate W);
int WebTemplate_write(WebTemplate W, char *name);
void WebTemplate_reset_output(WebTemplate W);
char *WebTemplate_html2text(char *s);
char *WebTemplate_text2html(char *s);
void WebTemplate_scan_arg(WebTemplate W, char *str);
void WebTemplate_set_comments(WebTemplate W, char *start, char *end);
char *WebTemplate_get_error_string(WebTemplate W);

extern char *webtpl_version;

#endif /* LIBRARY */


#endif /* webtpl_h */
