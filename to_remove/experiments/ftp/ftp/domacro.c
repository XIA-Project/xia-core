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

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "ftp_var.h"

void
domacro (int argc, char *argv[])
{
  int i, j, count = 2, loopflg = 0;
  char *cp1, *cp2, line2[200];
  struct cmd *c;

  if (argc < 2 && !another (&argc, &argv, "macro name"))
    {
      printf ("Usage: %s macro_name.\n", argv[0]);
      code = -1;
      return;
    }
  for (i = 0; i < macnum; ++i)
    {
      if (!strncmp (argv[1], macros[i].mac_name, 9))
	{
	  break;
	}
    }
  if (i == macnum)
    {
      printf ("'%s' macro not found.\n", argv[1]);
      code = -1;
      return;
    }
  strcpy (line2, line);

  do
    {
      cp1 = macros[i].mac_start;
      while (cp1 != macros[i].mac_end)
	{
	  while (isspace (*cp1))
	    {
	      cp1++;
	    }
	  cp2 = line;
	  while (*cp1 != '\0')
	    {
	      switch (*cp1)
		{
		case '\\':
		  *cp2++ = *++cp1;
		  break;
		case '$':
		  if (isdigit (*(cp1 + 1)))
		    {
		      j = 0;
		      while (isdigit (*++cp1))
			j = 10 * j + *cp1 - '0';
		      cp1--;
		      if (argc - 2 >= j)
			{
			  strcpy (cp2, argv[j + 1]);
			  cp2 += strlen (argv[j + 1]);
			}
		      break;
		    }
		  if (*(cp1 + 1) == 'i')
		    {
		      loopflg = 1;
		      cp1++;
		      if (count < argc)
			{
			  strcpy (cp2, argv[count]);
			  cp2 += strlen (argv[count]);
			}
		      break;
		    }
		  /* intentional drop through */
		default:
		  *cp2++ = *cp1;
		  break;
		}
	      if (*cp1 != '\0')
		cp1++;
	    }
	  *cp2 = '\0';
	  makeargv ();
	  if (margv[0] == NULL)
	    return;
	  c = getcmd (margv[0]);

	  if (c == (struct cmd *) -1)
	    {
	      printf ("?Ambiguous command\n");
	      code = -1;
	    }
	  else if (c == 0)
	    {
	      printf ("?Invalid command\n");
	      code = -1;
	    }
	  else if (c->c_conn && !connected)
	    {
	      printf ("Not connected.\n");
	      code = -1;
	    }
	  else
	    {
	      if (verbose)
		printf ("%s\n", line);
	      (*c->c_handler) (margc, margv);
	      if (bell && c->c_bell)
		putchar ('\007');
	      strcpy (line, line2);
	      makeargv ();
	      argc = margc;
	      argv = margv;
	    }
	  if (cp1 != macros[i].mac_end)
	    cp1++;
	}
    }
  while (loopflg && ++count < argc);
}
