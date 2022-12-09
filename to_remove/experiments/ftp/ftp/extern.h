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
 * Copyright (c) 1994
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

struct timeval;
struct fd_set;

void abort_remote (FILE *);
void abortpt ();
void abortrecv ();
void abortsend ();
void account (int, char **);
int another (int *, char ***, const char *);
void blkfree (char **);
void cd (int, char **);
void cdup (int, char **);
void changetype (int, int);
void cmdabort ();
void cmdscanner (int);
int command (const char *fmt, ...);
int confirm (char *, char *);
FILE *dataconn (char *);
void delete (int, char **);
void disconnect (int, char **);
void do_chmod (int, char **);
/*****************/
void do_chunk (int, char **);
/*****************/
void do_umask (int, char **);
void domacro (int, char **);
char *domap (char *);
void doproxy (int, char **);
char *dotrans (char *);
void fatal (char *);
void get (int, char **);
struct cmd *getcmd (char *);
int getit (int, char **, int, char *);
int getreply (int);
char *globulize (char *);
char *gunique (char *);
void help (int, char **);
char *hookup (char *, int);
void site_idle (int, char **);
int initconn (void);
void intr ();
void lcd (int, char **);
int login (char *);
void lostpeer ();
void ls (int, char **);
void mabort (int);
void macdef (int, char **);
void makeargv (void);
void makedir (int, char **);
void mdelete (int, char **);
void mget (int, char **);
void mls (int, char **);
void modtime (int, char **);
void mput (int, char **);
char *onoff (int);
void newer (int, char **);
void proxabort ();
void proxtrans (char *, char *, char *);
void psabort ();
void pswitch (int);
void ptransfer (char *, long, struct timeval *, struct timeval *);
void put (int, char **);
void pwd (int, char **);
void quit (int, char **);
void quote (int, char **);
void quote1 (char *, int, char **);
void recvrequest (char *, char *, char *, char *, int);
void reget (int, char **);
char *remglob (char **, int);
void removedir (int, char **);
void renamefile (int, char **);
void reset (int, char **);
void restart (int, char **);
void rmthelp (int, char **);
void rmtstatus (int, char **);
int ruserpass (char *, char **, char **, char **);
void sendrequest (char *, char *, char *, int);
void setascii (int, char **);
void setbell (int, char **);
void setbinary (int, char **);
void setcase (int, char **);
void setcr (int, char **);
void setdebug (int, char **);
void setform (int, char **);
void setftmode (int, char **);
void setglob (int, char **);
void sethash (int, char **);
void setnmap (int, char **);
void setntrans (int, char **);
void setpassive (int, char **);
void setpeer (int, char **);
void setport (int, char **);
void setprompt (int, char **);
void setrunique (int, char **);
void setstruct (int, char **);
void setsunique (int, char **);
void settenex (int, char **);
void settrace (int, char **);
void settype (int, char **);
void setverbose (int, char **);
void shell (int, char **);
void site (int, char **);
void sizecmd (int, char **);
char *slurpstring (void);
void status (int, char **);
void syst (int, char **);
void tvsub (struct timeval *, struct timeval *, struct timeval *);
void user (int, char **);

extern jmp_buf abortprox;
extern int abrtflag;
extern FILE *cout;
extern int data;
extern char *home;
extern jmp_buf jabort;
extern int proxy;
extern char reply_string[];
extern off_t restart_point;
extern int NCMDS;
