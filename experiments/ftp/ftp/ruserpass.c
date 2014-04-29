/*
  Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
  2004, 2005, 2006, 2007, 2008, 2009, 2010 Free Software Foundation,
  Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

/*
 * Copyright (c) 1985, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <error.h>

#include "ftp_var.h"
#include <libinetutils.h>
#include <xalloc.h>

static int token (void);
static FILE *cfile;

/* protect agains os headers */
#undef	DEFAULT
#define DEFAULT	1
#undef	LOGIN
#define LOGIN	2
#undef	PASSWD
#define PASSWD	3
#undef	ACCOUNT
#define ACCOUNT 4
#undef  MACDEF
#define MACDEF  5
#undef	ID
#define ID	10
#undef	MACHINE
#define MACHINE	11

static char tokval[100];

static struct toktab
{
  char *tokstr;
  int tval;
} toktab[] =
{
  {
  "default", DEFAULT},
  {
  "login", LOGIN},
  {
  "password", PASSWD},
  {
  "passwd", PASSWD},
  {
  "account", ACCOUNT},
  {
  "machine", MACHINE},
  {
  "macdef", MACDEF},
  {
  NULL, 0}
};

int
ruserpass (char *host, char **aname, char **apass, char **aacct)
{
  char *hdir, buf[BUFSIZ], *tmp;
  char *myname = 0, *mydomain;
  int t, i, c, usedefault = 0;
  struct stat stb;

  hdir = getenv ("HOME");
  if (hdir == NULL)
    hdir = ".";
  snprintf (buf, sizeof buf, "%s/.netrc", hdir);
  cfile = fopen (buf, "r");
  if (cfile == NULL)
    {
      if (errno != ENOENT)
	error (0, errno, "%s", buf);
      return (0);
    }

  myname = localhost ();
  if (!myname)
    myname = xstrdup ("");

  mydomain = strchr (myname, '.');
  if (mydomain == NULL)
    mydomain = xstrdup ("");
 next:
  while ((t = token ()))
    switch (t)
      {
      case DEFAULT:
	usedefault = 1;
	/* FALL THROUGH */

      case MACHINE:
	if (!usedefault)
	  {
	    if (token () != ID)
	      continue;
	    /*
	     * Allow match either for user's input host name
	     * or official hostname.  Also allow match of
	     * incompletely-specified host in local domain.  */
	    if (strcasecmp (host, tokval) == 0)
	      goto match;
	    if (strcasecmp (hostname, tokval) == 0)
	      goto match;
	    if ((tmp = strchr (hostname, '.')) != NULL
		&& strcasecmp (tmp, mydomain) == 0
		&& strncasecmp (hostname, tokval, tmp - hostname) == 0
		&& tokval[tmp - hostname] == '\0')
	      goto match;
	    if ((tmp = strchr (host, '.')) != NULL
		&& strcasecmp (tmp, mydomain) == 0
		&& strncasecmp (host, tokval, tmp - host) == 0
		&& tokval[tmp - host] == '\0')
	      goto match;
	    continue;
	  }
      match:
	while ((t = token ()) && t != MACHINE && t != DEFAULT)
	  switch (t)
	    {
	    case LOGIN:
	      if (token ())
                {
                  if (*aname == 0)
                    {
                      *aname = xmalloc ((unsigned) strlen (tokval) + 1);
                      strcpy (*aname, tokval);
                    }
                  else
                    {
                      if (strcmp (*aname, tokval))
                        goto next;
                    }
                }
	      break;
	    case PASSWD:
	      if ((*aname == NULL || strcmp (*aname, "anonymous"))
		  && fstat (fileno (cfile), &stb) >= 0
		  && (stb.st_mode & 077) != 0)
		{
		  error (0, 0, "Error: .netrc file is readable by others.");
		  error (0, 0,
			 "Remove password or make file unreadable by others.");
		  goto bad;
		}
	      if (token () && *apass == 0)
		{
		  *apass = xmalloc ((unsigned) strlen (tokval) + 1);
		  strcpy (*apass, tokval);
		}
	      break;
	    case ACCOUNT:
	      if (fstat (fileno (cfile), &stb) >= 0
		  && (stb.st_mode & 077) != 0)
		{
		  error (0, 0, "Error: .netrc file is readable by others.");
		  error (0, 0,
			 "Remove account or make file unreadable by others.");
		  goto bad;
		}
	      if (token () && *aacct == 0)
		{
		  *aacct = xmalloc ((unsigned) strlen (tokval) + 1);
		  strcpy (*aacct, tokval);
		}
	      break;
	    case MACDEF:
	      if (proxy)
		goto done;

	      while (((c = getc (cfile)) != EOF && c == ' ') || c == '\t')
		;
	      if (c == EOF || c == '\n')
		{
		  printf ("Missing macdef name argument.\n");
		  goto bad;
		}
	      if (macnum == 16)
		{
		  printf ("Limit of 16 macros have already been defined\n");
		  goto bad;
		}
	      tmp = macros[macnum].mac_name;
	      *tmp++ = c;
	      for (i = 0; i < 8 && (c = getc (cfile)) != EOF && !isspace (c);
		   ++i)
		{
		  *tmp++ = c;
		}
	      if (c == EOF)
		{
		  printf ("Macro definition missing null line terminator.\n");
		  goto bad;
		}
	      *tmp = '\0';
	      if (c != '\n')
		{
		  while ((c = getc (cfile)) != EOF && c != '\n');
		}
	      if (c == EOF)
		{
		  printf ("Macro definition missing null line terminator.\n");
		  goto bad;
		}
	      if (macnum == 0)
		{
		  macros[macnum].mac_start = macbuf;
		}
	      else
		{
		  macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
		}
	      tmp = macros[macnum].mac_start;
	      while (tmp != macbuf + 4096)
		{
		  if ((c = getc (cfile)) == EOF)
		    {
		      printf
			("Macro definition missing null line terminator.\n");
		      goto bad;
		    }
		  *tmp = c;
		  if (*tmp == '\n')
		    {
		      if (*(tmp - 1) == '\0')
			{
			  macros[macnum++].mac_end = tmp - 1;
			  break;
			}
		      *tmp = '\0';
		    }
		  tmp++;
		}
	      if (tmp == macbuf + 4096)
		{
		  printf ("4K macro buffer exceeded\n");
		  goto bad;
		}
	      break;
	    default:
	      error (0, 0, "Unknown .netrc keyword %s", tokval);
	      break;
	    }
	goto done;
      }
done:
  fclose (cfile);
  free (myname);
  return (0);
bad:
  fclose (cfile);
  free (myname);
  return (-1);
}

static int
token (void)
{
  char *cp;
  int c;
  struct toktab *t;

  if (feof (cfile) || ferror (cfile))
    return (0);
  while ((c = getc (cfile)) != EOF &&
	 (c == '\n' || c == '\t' || c == ' ' || c == ','))
    continue;
  if (c == EOF)
    return (0);
  cp = tokval;
  if (c == '"')
    {
      while ((c = getc (cfile)) != EOF && c != '"')
	{
	  if (c == '\\')
	    c = getc (cfile);
	  *cp++ = c;
	}
    }
  else
    {
      *cp++ = c;
      while ((c = getc (cfile)) != EOF
	     && c != '\n' && c != '\t' && c != ' ' && c != ',')
	{
	  if (c == '\\')
	    c = getc (cfile);
	  *cp++ = c;
	}
    }
  *cp = 0;
  if (tokval[0] == 0)
    return (0);
  for (t = toktab; t->tokstr; t++)
    if (!strcmp (t->tokstr, tokval))
      return (t->tval);
  return (ID);
}
