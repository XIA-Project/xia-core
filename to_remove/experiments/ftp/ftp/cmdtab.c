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
 * Copyright (c) 1985, 1989, 1993, 1994
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

#include <stdio.h>
#include <string.h>

#include "ftp_var.h"

/*
 * User FTP -- Command Tables.
 */

char accounthelp[] = "send account command to remote server";
char appendhelp[] = "append to a file";
char asciihelp[] = "set ascii transfer type";
char beephelp[] = "beep when command completed";
char binaryhelp[] = "set binary transfer type";
char casehelp[] = "toggle mget upper/lower case id mapping";
char cdhelp[] = "change remote working directory";
char cduphelp[] = "change remote working directory to parent directory";
char chmodhelp[] = "change file permissions of remote file";
/*****************/
char chunkhelp[] = "toggle chunking";
/*****************/
char connecthelp[] = "connect to remote tftp";
char crhelp[] = "toggle carriage return stripping on ascii gets";
char deletehelp[] = "delete remote file";
char debughelp[] = "toggle/set debugging mode";
char dirhelp[] = "list contents of remote directory";
char disconhelp[] = "terminate ftp session";
char domachelp[] = "execute macro";
char formhelp[] = "set file transfer format";
char globhelp[] = "toggle metacharacter expansion of local file names";
char hashhelp[] = "toggle printing `#' for each buffer transferred";
char helphelp[] = "print local help information";
char idlehelp[] = "get (set) idle timer on remote side";
char lcdhelp[] = "change local working directory";
char lshelp[] = "list contents of remote directory";
char macdefhelp[] = "define a macro";
char mdeletehelp[] = "delete multiple files";
char mdirhelp[] = "list contents of multiple remote directories";
char mgethelp[] = "get multiple files";
char mkdirhelp[] = "make directory on the remote machine";
char mlshelp[] = "list contents of multiple remote directories";
char modtimehelp[] = "show last modification time of remote file";
char modehelp[] = "set file transfer mode";
char mputhelp[] = "send multiple files";
char newerhelp[] = "get file if remote file is newer than local file ";
char nlisthelp[] = "nlist contents of remote directory";
char nmaphelp[] = "set templates for default file name mapping";
char ntranshelp[] = "set translation table for default file name mapping";
char porthelp[] = "toggle use of PORT cmd for each data connection";
char prompthelp[] = "force interactive prompting on multiple commands";
char proxyhelp[] = "issue command on alternate connection";
char pwdhelp[] = "print working directory on remote machine";
char quithelp[] = "terminate ftp session and exit";
char quotehelp[] = "send arbitrary ftp command";
char receivehelp[] = "receive file";
char regethelp[] = "get file restarting at end of local file";
char remotehelp[] = "get help from remote server";
char renamehelp[] = "rename file";
char restarthelp[] = "restart file transfer at bytecount";
char rmdirhelp[] = "remove directory on the remote machine";
char rmtstatushelp[] = "show status of remote machine";
char runiquehelp[] = "toggle store unique for local files";
char resethelp[] = "clear queued command replies";
char sendhelp[] = "send one file";
char passivehelp[] = "enter passive transfer mode";
char sitehelp[] =
  "send site specific command to remote server\n\t\tTry \"rhelp site\" or \"site help\" for more information";
char shellhelp[] = "escape to the shell";
char sizecmdhelp[] = "show size of remote file";
char statushelp[] = "show current status";
char structhelp[] = "set file transfer structure";
char suniquehelp[] = "toggle store unique on remote machine";
char systemhelp[] = "show remote system type";
char tenexhelp[] = "set tenex file transfer type";
char tracehelp[] = "toggle packet tracing";
char typehelp[] = "set file transfer type";
char umaskhelp[] = "get (set) umask on remote side";
char userhelp[] = "send new user information";
char verbosehelp[] = "toggle verbose mode";

static struct cmd cmdtab[] = {
  {"!", shellhelp, 0, 0, 0, shell},
  {"$", domachelp, 1, 0, 0, domacro},
  {"account", accounthelp, 0, 1, 1, account},
  {"append", appendhelp, 1, 1, 1, put},
  {"ascii", asciihelp, 0, 1, 1, setascii},
  {"bell", beephelp, 0, 0, 0, setbell},
  {"binary", binaryhelp, 0, 1, 1, setbinary},
  {"bye", quithelp, 0, 0, 0, quit},
  {"case", casehelp, 0, 0, 1, setcase},
  {"cd", cdhelp, 0, 1, 1, cd},
  {"cdup", cduphelp, 0, 1, 1, cdup},
  {"chmod", chmodhelp, 0, 1, 1, do_chmod},
/*****************/
  {"chunk", chunkhelp, 0, 1, 1, do_chunk},
/*****************/
  {"close", disconhelp, 0, 1, 1, disconnect},
  {"cr", crhelp, 0, 0, 0, setcr},
  {"delete", deletehelp, 0, 1, 1, delete},
  {"debug", debughelp, 0, 0, 0, setdebug},
  {"dir", dirhelp, 1, 1, 1, ls},
  {"disconnect", disconhelp, 0, 1, 1, disconnect},
  {"form", formhelp, 0, 1, 1, setform},
  {"get", receivehelp, 1, 1, 1, get},
  {"glob", globhelp, 0, 0, 0, setglob},
  {"hash", hashhelp, 0, 0, 0, sethash},
  {"help", helphelp, 0, 0, 1, help},
  {"idle", idlehelp, 0, 1, 1, site_idle},
  {"image", binaryhelp, 0, 1, 1, setbinary},
  {"lcd", lcdhelp, 0, 0, 0, lcd},
  {"ls", lshelp, 1, 1, 1, ls},
  {"macdef", macdefhelp, 0, 0, 0, macdef},
  {"mdelete", mdeletehelp, 1, 1, 1, mdelete},
  {"mdir", mdirhelp, 1, 1, 1, mls},
  {"mget", mgethelp, 1, 1, 1, mget},
  {"mkdir", mkdirhelp, 0, 1, 1, makedir},
  {"mls", mlshelp, 1, 1, 1, mls},
  {"mode", modehelp, 0, 1, 1, setftmode},
  {"modtime", modtimehelp, 0, 1, 1, modtime},
  {"mput", mputhelp, 1, 1, 1, mput},
  {"newer", newerhelp, 1, 1, 1, newer},
  {"nmap", nmaphelp, 0, 0, 1, setnmap},
  {"nlist", nlisthelp, 1, 1, 1, ls},
  {"ntrans", ntranshelp, 0, 0, 1, setntrans},
  {"open", connecthelp, 0, 0, 1, setpeer},
  {"passive", passivehelp, 0, 0, 0, setpassive},
  {"prompt", prompthelp, 0, 0, 0, setprompt},
  {"proxy", proxyhelp, 0, 0, 1, doproxy},
  {"sendport", porthelp, 0, 0, 0, setport},
  {"put", sendhelp, 1, 1, 1, put},
  {"pwd", pwdhelp, 0, 1, 1, pwd},
  {"quit", quithelp, 0, 0, 0, quit},
  {"quote", quotehelp, 1, 1, 1, quote},
  {"recv", receivehelp, 1, 1, 1, get},
  {"reget", regethelp, 1, 1, 1, reget},
  {"rstatus", rmtstatushelp, 0, 1, 1, rmtstatus},
  {"rhelp", remotehelp, 0, 1, 1, rmthelp},
  {"rename", renamehelp, 0, 1, 1, renamefile},
  {"reset", resethelp, 0, 1, 1, reset},
  {"restart", restarthelp, 1, 1, 1, restart},
  {"rmdir", rmdirhelp, 0, 1, 1, removedir},
  {"runique", runiquehelp, 0, 0, 1, setrunique},
  {"send", sendhelp, 1, 1, 1, put},
  {"site", sitehelp, 0, 1, 1, site},
  {"size", sizecmdhelp, 1, 1, 1, sizecmd},
  {"status", statushelp, 0, 0, 1, status},
  {"struct", structhelp, 0, 1, 1, setstruct},
  {"system", systemhelp, 0, 1, 1, syst},
  {"sunique", suniquehelp, 0, 0, 1, setsunique},
  {"tenex", tenexhelp, 0, 1, 1, settenex},
  {"trace", tracehelp, 0, 0, 0, settrace},
  {"type", typehelp, 0, 1, 1, settype},
  {"user", userhelp, 0, 1, 1, user},
  {"umask", umaskhelp, 0, 1, 1, do_umask},
  {"verbose", verbosehelp, 0, 0, 0, setverbose},
  {"?", helphelp, 0, 0, 1, help},
  {0},
};

#define NCMDS (sizeof (cmdtab) / sizeof (cmdtab[0]) - 1)

struct cmd *
getcmd (char *name)
{
  char *p, *q;
  struct cmd *c, *found;
  int nmatches, longest;

  longest = 0;
  nmatches = 0;
  found = 0;
  for (c = cmdtab; (p = c->c_name); c++)
    {
      for (q = name; *q == *p++; q++)
	if (*q == 0)		/* exact match? */
	  return c;
      if (!*q)
	{			/* the name was a prefix */
	  if (q - name > longest)
	    {
	      longest = q - name;
	      nmatches = 1;
	      found = c;
	    }
	  else if (q - name == longest)
	    nmatches++;
	}
    }
  if (nmatches > 1)
    return (struct cmd *) -1;
  return found;
}

#define HELPINDENT ((int) sizeof ("directory"))

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help (int argc, char *argv[])
{
  struct cmd *c;

  if (argc == 1)
    {
      int i, j, w, k;
      int columns, width = 0, lines;

      printf ("Commands may be abbreviated.  Commands are:\n\n");
      for (c = cmdtab; c < &cmdtab[NCMDS]; c++)
	{
	  int len = strlen (c->c_name);

	  if (len > width)
	    width = len;
	}
      width = (width + 8) & ~7;
      columns = 80 / width;
      if (columns == 0)
	columns = 1;
      lines = (NCMDS + columns - 1) / columns;
      for (i = 0; i < lines; i++)
	{
	  for (j = 0; j < columns; j++)
	    {
	      c = cmdtab + j * lines + i;
	      if (c->c_name && (!proxy || c->c_proxy))
		{
		  printf ("%s", c->c_name);
		}
	      else if (c->c_name)
		{
		  for (k = 0; k < strlen (c->c_name); k++)
		    putchar (' ');
		}
	      if (c + lines >= &cmdtab[NCMDS])
		{
		  printf ("\n");
		  break;
		}
	      w = strlen (c->c_name);
	      while (w < width)
		{
		  w = (w + 8) & ~7;
		  putchar ('\t');
		}
	    }
	}
      return;
    }

  while (--argc > 0)
    {
      char *arg;
      arg = *++argv;
      c = getcmd (arg);
      if (c == (struct cmd *) -1)
	printf ("?Ambiguous help command %s\n", arg);
      else if (c == (struct cmd *) 0)
	printf ("?Invalid help command %s\n", arg);
      else
	printf ("%-*s\t%s\n", HELPINDENT, c->c_name, c->c_help);
    }
}
