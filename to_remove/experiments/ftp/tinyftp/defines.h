/***************************************************************************
 *            defines.h
 *
 *  Copyright 2005 Dimitur Kirov
 *  dkirov@gmail.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _DEFINES_H
#define _DEFINES_H
#include "Xsocket.h"

#ifdef __cplusplus
extern "C"
{
#endif
	
	#define bool unsigned short
	enum {
		TRUE=1,
		FALSE=0
	};
	typedef struct cmd_opts {
		bool daemonize;
		bool listen_any;
		char *listen_addr;
		int port;
		int max_conn;
		int userid;
		char *chrootdir;
	}cmd_opts;
	
	int toint(const char*,bool);
	#define FILE_SEPARATOR "/"
	#define DEFAULT_PORT 21
	#define ANON_USER "anonymous"
	#define EOL "\r\n"
	#define RETRY_MINUTES 2 
	#define RCVBUFSIZE 512
	#define CMDBUFSIZE 5
	#define DATABUFSIZE 498
	#define SENDBUFSIZE 512
	#define PUTBUFSIZE 65000
	#define ERR_CONNECT 301
	#define ADDRBUFSIZE 16
	#define MAXPATHLEN 256
	#define PORTDELIM ","
	
	/**
	 * Response messages. 
	 */
	#define REPL_110 "110 Restart marker reply.\r\n"
	#define REPL_120 "120 Try again in 2 minutes.\r\n"
	#define REPL_125 "125 Data connection already open; transfer starting.\r\n"
	#define REPL_150 "150 File status okay; about to open data connection.\r\n"
	#define REPL_200 "200 Command okay.\r\n"
	#define REPL_202 "202 Command not implemented, superfluous at this site.\r\n"
	#define REPL_211 "221 System status, or system help reply.\r\n"
	#define REPL_211_STATUS "221-status of %s.\r\n"
	#define REPL_211_END "221 End of status.\r\n"
	#define REPL_212 "212 Directory status.\r\n"
	#define REPL_213 "213 File status.\r\n"
	#define REPL_214 "214 Help message.\r\n"
	#define REPL_214_END "214 End Help message.\r\n"
	#define REPL_215 "215 %s system type.\r\n"
	#define REPL_220 "220 Service ready for new user.\r\n"
	#define REPL_221 "221 Service closing control connection.\r\n"
	#define REPL_225 "225 Data connection open; no transfer in progress.\r\n"
	#define REPL_226 "226 Closing data connection.\r\n"
	#define REPL_227 "227 Entering Passive Mode (%s,%s,%s,%s,%s,%s).\r\n"
	#define REPL_230 "230 User logged in, proceed.\r\n"
	#define REPL_250 "250 Requested file action okay, completed.\r\n"
	#define REPL_257 "257 %s created.\r\n"
	#define REPL_257_PWD "257 \"%s\" is current working dir.\r\n"
	#define REPL_331 "331 Only anonymous user is accepted.\r\n"
	#define REPL_331_ANON "331 Anonymous login okay, send your complete email as your password.\r\n"
	#define REPL_332 "332 Need account for login.\r\n"
	#define REPL_350 "350 Requested file action pending further information.\r\n"
	#define REPL_421 "421 Service not available, closing control connection.\r\n"
	#define REPL_425 "425 Can't open data connection.\r\n"
	#define REPL_426 "426 Connection closed; transfer aborted.\r\n"
	#define REPL_450 "450 Requested file action not taken.\r\n"
	#define REPL_451 "451 Requested action aborted. Local error in processing.\r\n"
	#define REPL_452 "452 Requested action not taken.\r\n"
	#define REPL_500 "500 Syntax error, command unrecognized.\r\n"
	#define REPL_501 "501 Syntax error in parameters or arguments.\r\n"
	#define REPL_502 "502 Command not implemented.\r\n"
	#define REPL_503 "503 Bad sequence of commands.\r\n"
	#define REPL_504 "504 Command not implemented for that parameter.\r\n"
	#define REPL_530 "530 Not logged in.\r\n"
	#define REPL_532 "532 Need account for storing files.\r\n"
	#define REPL_550 "550 Requested action not taken.\r\n"
	#define REPL_551 "551 Requested action aborted. Page type unknown.\r\n"
	#define REPL_552 "552 Requested file action aborted.\r\n"
	#define REPL_553 "553 Requested action not taken.\r\n"
	
	
	//int make_client_connection(int, int ,const char* );
	int make_client_connection(int, char* );
	void close_conn(int );
	//int get_command(int conn_fd,char *read_buff1,char *data_buff);
	int send_repl_client(int ,char *);
	int send_repl_client_len(int ,char *,int );
	int send_repl_len(int ,char *,int );
	int close_connection(int );
	int create_socket(struct cmd_opts *);
	int pars_cmd_args(struct cmd_opts *,int argc, char *argv[]);
	int send_repl(int send_sock,char *msg);
	/*ChunkInfo *chunkinfo = NULL;
	ChunkContext *ctx; */
	
#ifdef __cplusplus
}
#endif

#endif /* _DEFINES_H */
