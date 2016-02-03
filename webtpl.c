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


/* (webtpl) c template library

 Version: 1.13, October 12, 2010

 by Jim Fox, fox@washington.edu

 */

#ifndef WIN32
#define SLEEP sleep(1)
#else 
#include <Windows.h>
#include <ctype.h>
#include <io.h>
#define open _open
#define close _close
#define SLEEP Sleep(1000)
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LIBRARY
#include "webtpl.h"

// just in case somebody wants to know, from Makefile.am
char *webtpl_version = VERSION;

char *remote_user = NULL;


/* --- error handlers --- */

static void clear_error_string(WebTemplate W)
{
   if (W->error_string) free(W->error_string);
   W->error_string = NULL;
}

static void set_error_string(WebTemplate W, int err, char *msg)
{
   clear_error_string(W);
   if (msg) W->error_string = strdup(msg);
   else {
      W->error_string = (char*) malloc(512);
      strerror_r(err, W->error_string, 512);
      W->error_string[511] = '\0';
   }
}

/* ---- Macros -----------------*/

/* Macros have a unique name and a value. 
   Where they appear in the template the value is substituted. */

/* Find a macro by name */

static TmplMacro find_macro(TmplMacro M, char *name)
{
   TmplMacro m;
   for (m=M;m;m=m->next) if (!strcmp(m->name, name)) return (m);
   return (NULL);
}

/* Create a new macro. Value may be null. */

static TmplMacro malloc_macro(char *name)
{
   TmplMacro m = (TmplMacro) malloc(sizeof(TmplMacro_));
   memset(m,'\0',sizeof(TmplMacro_));
   m->name = strdup(name);
   return (m);
}

/* Define a macro with name and value.
   If value is null, keep old value,
   in case it was assigned before page parse.

   Note that the name data is copied, but the value pointer is copied.
   The value must have been malloc'd by the caller.  */

static TmplMacro add_macro(TmplMacro M, char *name, char *value)
{
   TmplMacro m;
   TmplMacro lm;

   for (m=M;m;lm=m,m=m->next) if (!strcmp(m->name, name)) break;
   if (!m) {
      m = malloc_macro(name);
      lm->next = m;
   } else {
      if (!value) return (m);
      if (m->value) free(m->value);
   }
   m->value = value;
   if (value) m->len = strlen(value);
   else m->len = 0;
   return (m);
}

/* Append a macro to the list end.  (For multi-value, i.e. arg, macros.)

   Note that the name data is copied, but the value pointer is referenced.
   The value must have been malloc'd by the caller.  */

static TmplMacro append_macro_b(TmplMacro M,
                char *name, char *value, size_t len)
{
   TmplMacro m;
   TmplMacro lm;

   for (m=M;m;lm=m,m=m->next);
   m = malloc_macro(name);
   lm->next = m;
   m->value = value;
   m->len = len;
   return (m);
}

static TmplMacro append_macro(TmplMacro M, char *name, char *value)
{
   TmplMacro m;
   if (value) m = append_macro_b(M, name, value, strlen(value));
   else m = append_macro_b(M, name, value, 0);
   return (m);
}

/* Free a macro chain, starting with the specified macro. */

static void free_macros(TmplMacro M)
{
   TmplMacro n;
   while (M) {
     n = M->next;
     if (M->name) free (M->name);
     if (M->value) free (M->value);
     if (M->xtra1) free (M->xtra1);
     if (M->xtra2) free (M->xtra2);
     free (M);
     M = n;
   }
}



/* ---- Templates -----------------*/


/* There are two types of templates:

   'plain': evaluates into a macro;
            usually read from a file
            identified by parent==NULL

   'dynamic': evaluates into a parents item list;
              always part of a parent template (dynamic block)
              identified by parent!=NULL

   All of the plain templates are linked together from the root.
   The dynamic templates are linked from a parent item list.

 */


/* Allocate a template structure */

static Template new_template(WebTemplate W, char *name, TmplItem pip)
{
   Template N = (Template) malloc(sizeof(Template_));
   Template t;
   N->next = NULL;
   N->pip = pip;
   N->base = (void*) W;
   if (!pip) {   /* if plain link to root */
      if (t=W->template) {
         while (t->next) t = t->next;
         t->next = N;
      } else W->template = N;
   }
   N->name = strdup(name);
   N->item = NULL;
   N->last = NULL;
   return (N);
}

/* Find a template by name */

/* find the 'plain' part */
static Template find_plain_template(WebTemplate W, char *name)
{
   Template t;
   for (t=W->template;t;t=t->next) if (!strcmp(t->name, name)) return (t);
   return (NULL);
}

/* find the dynamic part */
static Template find_dynamic_template(Template B, char *name)
{
   TmplItem i;
   Template t;
   for (i=B->item;i;i=i->next) {
       if (i->type!=TI_DYNAMIC) continue;
       t = (Template) i->content;
       if (!strcmp(t->name, name)) return (t);
   }
   return (NULL);
}

/* name is: "base.dyn.dyn..." */
static Template find_template(WebTemplate W, char *name)
{
   Template bt;
   char *base = strdup(name);
   char *d = strchr(base,'.');
   if (d) {   /* dynamic block name */
      *d++ = '\0';
      bt = find_plain_template(W, base);
      /* find dynamic part */
      while (bt && d) {
         char *dn = strchr(d,'.');
         if (dn) *dn++ = '\0';
         bt = find_dynamic_template(bt, d);
         d = dn;
      }
   } else bt = find_plain_template(W,base);
   free(base);
   return (bt);
}
         

/* Allocate and append a new item to a template */

static TmplItem add_item(Template t, int type, void *content, size_t len)
{
   TmplItem n = (TmplItem) malloc(sizeof(TmplItem_));
   TmplItem te;
   n->next = NULL;
   n->parent = t;
   n->type = type;
   n->content = content;
   n->len = len;

   /* link this item to the end */
   te = t->last;
   if (te) {
      te->next = n;
      t->last = n;
   } else {
      t->item = n;
      t->last = n;
   }
   return (n);
}

/* Allocate and insert a new item into a template's item list
   Insert after 'tgt'. */

static TmplItem insert_item(TmplItem tgt, int type, void *content)
{
   TmplItem n;
   Template P;

   /* If we're appending a dtext to a dtext just append the content.
      This keeps the number of items smaller. */
   if (type==TI_DTEXT && tgt->type==TI_DTEXT) {
      int nlen = strlen(content);
      tgt->content = (char*) realloc(tgt->content, tgt->len + nlen + 1);
      strcpy(tgt->content+tgt->len, content);
      tgt->len += nlen;
      free(content);
      return (tgt);
   }
   n = (TmplItem) malloc(sizeof(TmplItem_));
   P = (Template) tgt->parent;
   n->next = tgt->next;
   tgt->next = n;
   n->parent = P;
   if (P->last == tgt) P->last = n;
   n->type = type;
   n->content = content;
   n->len = strlen(content);
   return (n);
}
   

/* Free a string of templates, starting with the specified template. */

static void free_templates(Template T)
{
  Template n;
  TmplItem i,j;
  
  while (T) {
     n = T->next;
     i = T->item;
     while (i) {
       j = i->next;
       if (i->content) {
         if (i->type==TI_TEXT || i->type==TI_DTEXT) free(i->content);
         else if (i->type==TI_DYNAMIC) free_templates((Template)i->content);
       }
       free(i);
       i = j;
     }


/***
if (T->pip) {
  printf("freeT name = %s, pip=%x\n", T->name, T->pip);
  printf("pip   content = %s\n", T->pip->content);

  free(T->pip);
} else {
  printf("freeT name = %s, pip=NULL\n", T->name);
}
**/
     if (T->name) free(T->name);
     free (T);
     T = n;
  }
  return;
}

/* Free a single template.  Called because of failure during
   the reading of a template form file, or because a template
   name was reused. */

static void free_template(WebTemplate W, Template T)
{
   Template nt = T->next;
   Template t = NULL;
   
   /* Free all the content of this template only */
   T->next = NULL;
   free_templates(T);

   if (T==W->template) W->template = nt;
   else for (t=W->template;t;t=t->next) if (T==t->next) break;
   if (t) t->next = nt;
}
   
/* -------- Template readers ------------- */

  
static char *read_dyn_name(char *in)
{
  char *n = strchr(in,':');

  if (!n++) return (NULL);
  while (isspace(*n)) n++;
  in = n;
  while (*in && (isalnum(*in)||(*in=='_'))) in++;
  *in = '\0';
  return (n);
}

/* process a line of the template file */

static Template read_line(Template T, char *line)
{
   char *m, *e;
   WebTemplate W = (WebTemplate) T->base;
   
   /* If in comments, look for end */
   if (W->cip) {
      if (!strncmp(line,W->cend,W->lcend)) W->cip = 0;
      return (T);
   }
   /* Look for comment line */
   if (W->cstart && !strncmp(line,W->cstart,W->lcstart)) {
      if (W->cend) W->cip = 1;
      return (T);
   }

   /* look for dynamic block start */
   for (m=line;*m&&*m==' ';m++);
   if ((!strncmp(m,"<!-- BEGIN DYNAMIC BLOCK:",25)) ||
       (!strncmp(m,"<!-- BDB:",9))) {
      char *dn = read_dyn_name(m);
      Template D;
      if (!T->last) add_item(T, TI_TEXT, NULL, 0); /* dyn needs insert point */ 
      D = new_template(W, dn, T->last);
      add_item(T, TI_DYNAMIC, D, 0);
      return (D);

   /* look for dynamic block end */
   } else if ((!strncmp(m,"<!-- END DYNAMIC BLOCK:",23)) ||
              (!strncmp(m,"<!-- EDB:",9))) {
      char *dn = read_dyn_name(m);
      if (strcmp(T->name, dn)) {
         char emsg[512];
         snprintf(emsg, 512, "Block %s ended with %s\n", T->name, dn);
         set_error_string(W, -1, emsg);
         return (NULL);
      }
      return (T->pip->parent);
     
   /* copy text and macros */
   } else {
      char *tm = line;
      while (m = strchr(tm,'{')) {
         char *v;
         for (v=0,e=m+1;*e && (isalnum(*e)||(*e=='_')); e++);
         if (*e == '=') {   /* have value assignment */
            char *b;
            /* value */
            *e++ = '\0';
            v = e;
            b = strchr(e, '}');
            if (b) e = b;
         }
         if (*e == '}') {
            /* macro item */
            *m++ = '\0';
            *e++ = '\0';
            add_item(T, TI_TEXT, (void*) strdup(line), strlen(line));
            if (v) v = strdup(v);
            add_item(T, TI_MACRO, (void*) add_macro(W->macros, m, v), 0);
            line = e;
            tm = e;
            continue;
         } 
         tm = e+1;
         continue;
      }
      add_item(T, TI_TEXT, (void*) strdup(line), strlen(line));
   }
   return (T);
}

/* Parse a template file
   Return 0 on success, else errno or -1 */

static int read_template_file(Template T, FILE *f)
{
   char line[8192];

   errno = 0;
   while (T && fgets(line, 8192, f)) T = read_line(T, line);

   if (!T) return (-1);

   if (errno && errno!=EBADF && errno!=EINVAL) {
      set_error_string((WebTemplate)T->base, errno, NULL);
      return (errno);
   }
   
   if (T->pip) {
      char emsg[512];
      snprintf(emsg, 512, "Block %s did not end", T->name);
      set_error_string((WebTemplate)T->base, -1, emsg);
      return (-1);
   }

   if (((WebTemplate)T->base)->cip) {
      char emsg[512];
      snprintf(emsg, 512, "Comment in %s did not end", T->name);
      set_error_string((WebTemplate)T->base, -1, emsg);
      return (-1);
   }
   return (0);
}

/* ------ API template calls -------- */

/* Create a web template */
WebTemplate WebTemplate_new()
{
   WebTemplate W = (WebTemplate) malloc(sizeof(WebTemplate_));
   W->template = NULL;
   W->macros = malloc_macro("-");
   W->arg = malloc_macro("-");
   W->in_cookie = malloc_macro("-");
   W->header = malloc_macro("-");
   W->octet = malloc_macro("-");
   W->header_sent = 0;
   W->fd = 1;
   W->cstart = NULL;
   W->cend = NULL;
   W->cip = 0;
   W->error_string = NULL;
   return (W);
}
WebTemplate newWebTemplate()
{
   return (WebTemplate_new());
}

void WebTemplate_free(WebTemplate W)
{
   if (W) {
     free_templates(W->template);
     free_macros(W->macros);
     free_macros(W->arg);
     free_macros(W->in_cookie);
     free_macros(W->header);
     free_macros(W->octet);
     if (W->error_string) free(W->error_string);
     if (W->cstart) free(W->cstart);
     if (W->cend) free(W->cend);
     free(W);
   }
}
void freeWebTemplate(WebTemplate W)
{
   WebTemplate_free(W);
}

/* set the output fd - default is stdout */
void WebTemplate_set_output(WebTemplate W, int fd)
{
   clear_error_string(W);
   W->fd = fd;
}

/* set for no headers - e.g. output to html file */
void WebTemplate_set_noheader(WebTemplate W)
{
   clear_error_string(W);
   W->header_sent = 1;
}

/* set comment designators  */
void WebTemplate_set_comments(WebTemplate W, char *start, char *end)
{
   clear_error_string(W);
   if (W->cstart) free(W->cstart);
   if (W->cend) free(W->cend);

   if (start && *start) {
      W->cstart = strdup(start);
      W->lcstart = strlen(start);
      if (end && *end) {
         W->cend = strdup(end);
         W->lcend = strlen(end);
      } else W->cend = NULL;
   } else W->cstart = NULL;
}


/* Assign a value to a macro.
   Null value clears the macro. */

void WebTemplate_assign(WebTemplate W, char *name, char *value)
{
   TmplMacro m;
   clear_error_string(W);
   if (name) {
      if (value && *value) add_macro(W->macros,name,strdup(value));
      else if ((m=find_macro(W->macros, name)) && m->value) {
         free(m->value);
         m->value = NULL;
         m->len = 0;
      }
   }
}


/* Assign an integer value to a macro. */

void WebTemplate_assign_int(WebTemplate W, char *name, int value)
{
   char v[16];
   clear_error_string(W);
   if (name) {
      sprintf(v, "%d", value);
      add_macro(W->macros,name,strdup(v));
   }
}


/* Load a template from an open socket */
int WebTemplate_get_by_fd(WebTemplate W, char *name, int fd)
{
   Template T;
   int ret;
   clear_error_string(W);
   FILE *f = fdopen(fd, "r");
   if (!f) return (errno);
   if (T=find_plain_template(W, name)) free_template(W, T);
   T = new_template(W, name, NULL);
   ret = read_template_file(T, f);
   fclose(f);
   if (ret==0) return (0);
   free_template(W, T);
   return(errno);
}

/* Load a template from an open file */
int WebTemplate_get_by_fp(WebTemplate W, char *name, FILE *f)
{
   Template T;
   int ret;
   clear_error_string(W);
   if (!f) return (errno);
   if (T=find_plain_template(W, name)) free_template(W, T);
   T = new_template(W, name, NULL);
   if ((ret=read_template_file(T, f))==0) return (ret);
   free_template(W, T);
   return(errno);
}

/* Load a template from a file */
int WebTemplate_get_by_name(WebTemplate W, char *name, char *filename)
{
   int s;
   int fd;
   clear_error_string(W);
   fd = open(filename,O_RDONLY,0);
   if (fd<0) {
      set_error_string(W, errno, NULL);
      return(errno);
   }
   s = WebTemplate_get_by_fd(W, name, fd);
   close(fd);
   return (s);
}


   
/* ------- Template evaluation routines ----------- */



/* Parse (evaluate) a template.  
   This adds the evaluated content to the parent.
   We also have to remove the dynamic content after use. */

static char *parse_template(Template T)
{
   char *r;
   size_t len = 0;
   TmplItem ti;
   TmplItem pi = NULL;
   char *e;
   
   clear_error_string((WebTemplate)T->base);
   
   /* see how much space we need */
   for (ti=T->item;ti;ti=ti->next) {
      if (ti->type==TI_TEXT || ti->type==TI_DTEXT) len += ti->len;
      else if (ti->type==TI_MACRO) len += ((TmplMacro)ti->content)->len;
   }

   /* copy text */
   r = (char*) malloc(len+1);
   e = r;
   *e = '\0';
   for (ti=T->item;ti;pi=ti,ti=ti->next) {
      if (ti->type==TI_TEXT || ti->type==TI_DTEXT) {
         if (!ti->content) continue;
         memcpy(e, ti->content, ti->len);
         e += ti->len;
         if (ti->type==TI_DTEXT) {
            free(ti->content);    /* release old dynamic content */
            if (pi) {
               pi->next = ti->next;
               free(ti);
               ti = pi;
            }
         }
      } else if (ti->type==TI_MACRO) {
         TmplMacro m = (TmplMacro) ti->content;
         if (m->value) {
            memcpy(e, m->value, m->len);
            e += m->len;
         }
      } else if (ti->type==TI_DYNAMIC) { /* make sure pip is reset */
         ((Template)ti->content)->pip = pi;
      }
   }
   *e = '\0';
   return (r);
}




/* Parse a dynamic block 
   This adds the evaluated template to the parent */

int WebTemplate_parse_dynamic(WebTemplate W, char *dname)
{
   Template T;
   char *v;

   clear_error_string(W);
   if (!(T=find_template(W, dname))) {
      set_error_string(W, 1, "template not found");
      return (1);
   }
   v = parse_template(T);
   if (v) T->pip = insert_item(T->pip, TI_DTEXT, v);
   return (0);
}

/* Parse a plain template 
   This defines a macro with the evaluated template as its value */

int WebTemplate_parse(WebTemplate W, char *mname, char *tname)
{
   Template T;
   char *v;

   clear_error_string(W);
   if (!(T=find_template(W, tname))) {
      set_error_string(W, 1, "template not found");
      return (1);
   }
   v = parse_template(T);
   if (v) add_macro(W->macros, mname, v);
   return (0);
}



/* ------------ Form arguments, parameteres, and cookie tools ---- */


#define PRINTF if(0)printf

/* De-html an arg string. Returns a malloc'd string. */

static char *html2text(char *s)
{
   size_t l;
   char *out;
   char *v;
   long int k;
   char hex[4];

   if ((!s)||!*s) return (strdup(""));
   l = strlen(s);
   out = (char*) malloc(l+1);
   v = out;
   while (*s) {
      switch (*s) {
        case '+': *v++ = ' ';
                  s++;
                  break;
        case '%': 
                  if (isxdigit(s[1])&&isxdigit(s[2])) {
                     hex[0] = *++s;
                     hex[1] = *++s;
                     hex[2] = '\0';
                     k = strtol(hex,0,16);
                     *v++ = (char)k;
                     s++;
                  } else {   // not a hex encoding
                     *v++ = *s++;
                  }
                  break;
        case 0x0d: s++;  /* cr */
                  break;
        default:  *v++ = *s++;
      }
   }
   *v-- = '\0';
   while ( (v>out) && (*v=='\n'||*v=='\r')) *v-- = '\0';
   return (out);
}

/* convert special chars to html */

static char *text2html(char *s)
{
   char *out;
   char *v;
   int n;

   for (n=0,v=s; v&&*v; v++) {
      if ((*v=='"') ||
          (*v=='<') ||  
          (*v=='>') ||  
          (*v=='&')) n++;
   }
   if (!n) return (strdup(s));  
  
   out = (char*) malloc(strlen(s)+(n*6));
   for (v=out; s&&*s; s++) {
      switch (*s) {
         case '"': strcpy(v, "&quot;"); v+=6; break;
         case '<': strcpy(v, "&lt;"); v+=4; break;
         case '>': strcpy(v, "&gt;"); v+=4; break;
         case '&': strcpy(v, "&amp;"); v+=5; break;
         default: *v++ = *s;
      }
   }
   *v = '\0';
   return (out);
}


/* parse args and values.  Duplicate names produce multiple values. */
static void scan_arg(WebTemplate W, char *str)
{
   char *a, *v;
   do {
      if (a = strchr(str,'&')) *a++ = '\0';
      if (*str) {
         if (v=strchr(str,'=')) {
            *v++ = '\0';
            v = html2text(v);
         } else v = strdup("");
         append_macro(W->arg, str, v);
      }
   } while (str = a);
}


/* parse cookie. Duplicates overwrite old value. */
static void scan_cookie(WebTemplate W, char *str)
{
   char *a, *v;
   do {
      while (*str==' ') str++;
      if (a = strchr(str,';')) *a++ = '\0';
      if (*str) {
         if (v=strchr(str,'=')) {
            *v++ = '\0';
         }
         add_macro(W->in_cookie, str, strdup(v));
      }
   } while (str = a);
}


/* find a string in some buffer */
static char *memstr(char *mem, size_t meml, char *str, size_t strl)
{
   char *p;
   for (p=memchr(mem,*str,meml); p;
       meml-=(p-mem+1),mem=p+1,p=memchr(mem,*str,meml))
           if (!strncmp(p,str,strl)) return (p);
   return (NULL);
}

/* Parse a multipart form.  Duplicate names produce multiple values. */

static void scan_mp_arg(WebTemplate W, char *str, int strl, char *b)
{
   char *a, *v, *s, *e;
   char *n,*eh;
   char *fn, *ct;
   size_t l;
   size_t lb = strlen(b);
   int octet;
   char *estr = str + strl;

   for (s=memstr(str,strl,b,lb); s && (e = memstr(s+lb,estr-s-lb,b,lb)); s=e) {
      s += lb;
      *e = '\0';
      if (*s=='\r') s++;

      /* Parse the header. We want the name, filename, and type. */
      eh = strstr(s,"\r\n\r\n");
      if (!eh) continue;
      *eh = '\0';
      n = strstr(s,"name=\"");
      if (!n) continue;
      n += 6;
      s = strchr(n,'"');
      if (!s) continue;
      *s++ = '\0';  /* n is name */

      fn = strstr(s,"filename=\"");
      if (fn) {
        fn += 10;
        s = strchr(fn,'"');
        if (s) *s++ = '\0';  /* fn is filename */
        else fn = NULL;
      }

      /* We'll assume any specified content is 'binary' */
      if (ct = strstr(s, "Content-Type:")) {
        octet = 1;
        ct += 14;
        s = strchr(ct,'\r');
        if (s) *s++ = '\0';  /* ct is content-type */
      } else octet = 0;
     
      s = eh+4;
      l = e - s - 2; /* data followed by \r\n */
      
      if (octet) {
         TmplMacro m;
         v = (char*) malloc(l);
         memcpy(v, s, l);
         m = append_macro_b(W->octet, n, v, l);
         m->xtra1 = strdup(fn);
         m->xtra2 = strdup(ct);
      } else {
         v = (char*) malloc(l+1); 
         for (a=v; *s; s++) if (*s!='\r') *a++=*s; /* delete CRs */
         *a = '\0';
         append_macro(W->arg, n, v);
      }
   } 
}

static char *months[] = {
 "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static char *wdays[] = {
 "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

void WebTemplate_set_cookie(WebTemplate W, char *name, char *argvalue,
        time_t argexp, char *argdomain, char *argpath, int secure)
{
   int l = 0;
   char *cv;
   char *v, *p, *d, *s, *e;

   clear_error_string(W);
   if (!name) return;
   if (argvalue) {
      v = argvalue;
   } else v = "";
   if (argpath) {
      p = (char*) malloc(strlen(argpath)+8);
      sprintf(p," path=%s;", argpath);
   } else p = "";
   if (argdomain) {
      d = (char*) malloc(strlen(argdomain)+10);
      sprintf(d," domain=%s;", argdomain);
   } else d = "";
   if (secure) {
      s = " secure";
   } else s = "";
   if (argexp) {
      struct tm *t;
      t = gmtime(&argexp);
      e = (char*) malloc(48);
      sprintf(e," expires=%s, %02d-%s-%4d %02d:%02d:%02d GMT;",
          wdays[t->tm_wday], t->tm_mday, months[t->tm_mon],
          t->tm_year+1900, t->tm_hour, t->tm_min, t->tm_sec);
   } else e = "";
   
   cv = (char*) malloc(strlen(name) + strlen(s) + 
        strlen(v) + strlen(p) + strlen(d) + strlen(e) + 16);
   sprintf(cv, "%s=%s;%s%s%s%s", name, v, e, p, d, s);
   
   append_macro(W->header, "Set-Cookie", cv);
   if (argpath) free(p);
   if (argdomain) free(d);
   if (argexp) free(e);
}


/* Add a header line */
void WebTemplate_add_header(WebTemplate W, char *name, char *value)
{
   int l = 0;

   clear_error_string(W);
   if (!name) return;
   if (!value) return;
   append_macro(W->header, name, strdup(value));
}


/* Load the args and cookies values */
void WebTemplate_get_args(WebTemplate W)
{
   char *env;
   int n,r;
   
   clear_error_string(W);
   /* release any existing args or in-cookies */
   free_macros(W->arg->next);
   W->arg->next = NULL;
   free_macros(W->in_cookie->next);
   W->in_cookie->next = NULL;

   env = getenv("REMOTE_USER");
   if ((env)&&(*env)) {
      PRINTF("Got User: %s\n", env);
      remote_user = strdup(env);
   } else remote_user = NULL;
     
     
   env = getenv("CONTENT_LENGTH");
   if ((env)&&(*env)) {
      n = atoi(env);
      env = getenv("CONTENT_TYPE");
      if ((env)&&(*env)) {
         PRINTF("<p>POST data (%d) bytes of %s\n", n, env);
         if (!strncmp(env,"application/x-www-form-urlencoded",33)) {
            int nr;
            env = (char *)malloc(n+1);
            for (nr=0;nr<n;nr+=r) {
              r = read(0,env+nr,n-nr);
              if (!r) {
                 if (errno==EAGAIN) SLEEP;
                 else break;
              }
            }
            if (nr>0) {
              env[nr] = '\0';
              scan_arg(W, env);
            }
            free(env);
         } else if (!strncmp(env,"multipart/form-data",19)) {
            char *mpb;
            char *b = strstr(env,"boundary=");
            if (b) {
              int nr;
              mpb = strdup(b+7);
              strncpy(mpb,"--",2);  /* boundary actually has 2 extra - */
              env = (char *)malloc(n+1);
              for (nr=0;nr<n;nr+=r) {
                r = read(0,env+nr,n-nr);
                if (!r) {
                   if (errno==EAGAIN) SLEEP;
                   else break;
                }
              }
              if (nr>0) scan_mp_arg(W, env, n, mpb);
              free (env);
            }
         }
      }
   }

   env = getenv("QUERY_STRING");
   if ((env)&&(*env)) {
      char *e = strdup(env);
      PRINTF("Got GET args\n");
      scan_arg(W, e);
      free(e);
   }

   env = getenv("HTTP_COOKIE");
   if ((env)&&(*env)) {
      char *e = strdup(env);
      PRINTF("Got cookies args\n");
      scan_cookie(W, e);
      free(e);
   }
   
}


/* Return the value of an arg macro.  Caller must free the string.  */
char *WebTemplate_get_arg(WebTemplate W, char *name)
{
   TmplMacro M;
   clear_error_string(W);
   M = find_macro(W->arg, name);
   if (M && M->value) return (strdup(M->value));
   return (NULL);
}

/* Return a list of values of an arg macro. */
char **WebTemplate_get_arg_list(WebTemplate W, char *name)
{
   TmplMacro M = W->arg;
   TmplMacro m;
   int nv = 0;
   char **list;
   char **lp;

   clear_error_string(W);
   /* count number of matches */
   for (m=M;m;m=m->next) if (!strcmp(m->name, name)) nv++;
   if (!nv) return (NULL);

   /* build list */
   list = (char**) malloc((nv+1)*sizeof(char*));
   lp = list;
   for (m=M;m;m=m->next) if (m->value && !strcmp(m->name, name)) {
      *lp = strdup(m->value);
      lp++;
   }
   *lp = NULL;
   return (list);
}

void WebTemplate_free_arg_list(char **list)
{
   char **lp;
   if (!list) return;
   for (lp=list;*lp;lp++) free(*lp);
   free(list);
}

/* Return the n'th arg and value.  Caller must free both strings.  
    'n' is updated. Note the first arg is void. */
char *WebTemplate_get_next_arg(WebTemplate W, int *n, char **v)
{
   TmplMacro m;
   int i;
   clear_error_string(W);
   for (i=0,m=W->arg;m&&i<=*n;i++,m=m->next);
   if (m) {
      if (v) *v = m->value? strdup(m->value): NULL;
      (*n)++;
      return (strdup(m->name));
   }
   return (NULL);
}


/* Return the value of a cookie macro.  Caller must free the string. */
char *WebTemplate_get_cookie(WebTemplate W, char *name)
{
   TmplMacro M;
   clear_error_string(W);
   M = find_macro(W->in_cookie, name);
   if (M && M->value) return (strdup(M->value));
   return (NULL);
}


/* Return the value of an octet macro.  Caller must not free the memory.  */
int WebTemplate_get_octet_arg(WebTemplate W, char *name, 
     void **value, size_t *len, char **type, char **filename)
{
   TmplMacro M;
   clear_error_string(W);
   M = find_macro(W->octet, name);
   if (M && value && len) {
      *value = (void*) M->value;
      *len = M->len;
      if (type) *type = strdup(M->xtra2); 
      if (filename) *filename = strdup(M->xtra1); 
      return (1);
   }
   return (0);
}




/* ----- API to do output -------- */

/* Write the html header plus any cookies */

static char html_header[] = "Content-type: text/html; charset=ISO-8859-1\n";

int WebTemplate_header(WebTemplate W)
{
   TmplMacro m;
   char *buf;
   size_t l, lbuf;

   clear_error_string(W);
   if (W->header_sent) return (0);
   buf = (char*) malloc(256);
   lbuf = 256;

   /* Make sure there is a content header */
   m = find_macro(W->header, "Content-type");
   if (!m) write(W->fd, html_header, (int)strlen(html_header));

   /* Write any extra headers - including cookies. */
   for (m=W->header; m; m=m->next) {
      if (!m->value) continue;
      l = strlen(m->name) + 3 + m->len;
      if ((l+1)>lbuf) {
         buf = (char*) realloc(buf, (l+1));
         lbuf = (l+1);
      }
      sprintf(buf,"%s: %s\n", m->name, m->value);
      write(W->fd, buf, (int)l);
   }
   free (buf);
   write(W->fd,"\n",1);

   W->header_sent = 1;
   return (0);
}



/* Write a macro value */

int WebTemplate_write(WebTemplate W, char *name)
{
   TmplMacro m = find_macro(W->macros, name);
   int s;

   clear_error_string(W);
   if (!W->header_sent) WebTemplate_header(W);
   if (!m || !m->value) return (-1);
   s = write(W->fd, m->value, (int)m->len);
   if (s!=m->len) {
      set_error_string(W, errno, NULL);
      return (errno);
   }
   return (0);
}
  

/* Reset the output functions.  For persistant cgi
   this allows a clean, new page. */

void WebTemplate_reset_output(WebTemplate W)
{
   clear_error_string(W);
   free_macros(W->header->next);
   W->header->next = NULL;
   W->header_sent = 0;

   free_macros(W->octet->next);
   W->octet->next = NULL;
}


/* -- convenience functions */

/* convert html character encoding to plaintext */
char *WebTemplate_html2text(char *s) 
{
   return (html2text(s));
}

/* convert plaintext to html 'safer' strings */
char *WebTemplate_text2html(char *s)
{
   return (text2html(s));
}

/* Scan an arbitrary arg string into a template */
void WebTemplate_scan_arg(WebTemplate W, char *str)
{
   clear_error_string(W);
   if (str) {
     char *a = strdup(str);
     scan_arg(W, a);
     free (a);
   }
}

char *WebTemplate_macro_value(WebTemplate W, char *name)
{
   TmplMacro m;
   clear_error_string(W);
   m = find_macro(W->macros, name);
   if (m && m->value) return (strdup(m->value));
   else return (NULL);
}


char *WebTemplate_get_error_string(WebTemplate W)
{
   return (W->error_string);
}



