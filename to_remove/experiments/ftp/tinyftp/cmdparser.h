/***************************************************************************
 *            cmdparser.h
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

#ifndef _CMDPARSER_H
#define _CMDPARSER_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdio.h>
#include <stddef.h>
#include "defines.h"

enum {
	CMD_USER,
	CMD_PASS,
	CMD_ACCT,
	CMD_CWD,
	CMD_CDUP,
	CMD_SMNT,
	CMD_QUIT,
	CMD_REIN,
	CMD_PORT,
	CMD_PASV,
	CMD_TYPE,
	CMD_STRU,
	CMD_MODE,
	CMD_RETR,
	CMD_STOR,
	CMD_STOU,
	CMD_APPE,
	CMD_ALLO,
	CMD_REST,
	CMD_RNFR,
	CMD_RNTO,
	CMD_ABOR,
	CMD_DELE,
	CMD_RMD,
	CMD_MKD,
	CMD_PWD,
	CMD_LIST,
	CMD_NLST,
	CMD_SITE,
	CMD_SYST,
	CMD_STAT,
	CMD_HELP,
	CMD_NOOP,
	CMD_UNKNOWN,
	CMD_EMPTY,
	CMD_CLOSE
};

/*
Statichni funkcii za optimalna obrabotka na vhodnie komandi
*/
static void set_data(char *src,char *dest) {
	//dest[0] = '\0';
	int i=0,y=0;
	bool hasbegun = FALSE;
	for(;;i++) {
		if(hasbegun==TRUE) {
			dest[y]=src[i];
			if(src[i]=='\0' || src[i]=='\n' || src[i]=='\r')
				break;
			y++;
		} else {
			if(src[i]==' ' || src[i]=='	')
				continue;
			else {
				dest[0]=src[i];
				y=1;
				hasbegun=TRUE;
			}
		}
	}
	dest[y]='\0';
}

static bool cmp2(char c1,char c2,char d1,char d2) {
	if(c1==d1 && c2 == d2)
		return TRUE;
	return FALSE;
}
static bool cmp3(char c1,char c2,char c3,char d1,char d2,char d3) {
	if(cmp2(c1,c2,d1,d2) && c3==d3)
		return TRUE;
	return FALSE;
}

static bool _cmp2(char c1,char c2,char *d,char *d1) {
	if(d[0]=='\0')
		return FALSE;
	if(d[1]=='\0')
		return FALSE;
	if(!cmp2(c1,c2,d[0],d[1])) 
		return FALSE;
	set_data(d+2,d1);
	return TRUE;
}
static bool _cmp1(char c1,char *d,char *d1) {
	if(c1 != d[0])
		return FALSE;
	set_data(d+1,d1);
	return TRUE;
}

static bool _cmp3(char c1,char c2,char c3,char *d,char *d1) {
	if(d[0]=='\0')
		return FALSE;
	if(d[1]=='\0')
		return FALSE;
	if(d[2]=='\0')
		return FALSE;
	if(!cmp3(c1,c2,c3,d[0],d[1],d[2])) 
		return FALSE;
	set_data(d+3,d1);
	return TRUE;
}
static int parse_input(char *input_buff,char *data_buff) {
	if(input_buff==NULL)
		return CMD_EMPTY;

	//To avoid Warning "not used"
	parse_input(NULL,NULL); 
	
	int len = strlen(input_buff);
	if(len<3)
		return CMD_UNKNOWN;
	switch(input_buff[0]) {
		case 'A':
			switch(input_buff[1]) {
				case 'B':
					if(_cmp2('O','R',input_buff+2,data_buff)) 
						return CMD_ABOR;
				case 'C':
					if(_cmp2('C','T',input_buff+2,data_buff)) 
						return CMD_ACCT;
				case 'L':
					if(_cmp2('L','O',input_buff+2,data_buff)) 
						return CMD_ALLO;
				case 'P':
					if(_cmp2('P','E',input_buff+2,data_buff)) 
						return CMD_APPE;
				return CMD_UNKNOWN;
			}
		case 'C':
			if(_cmp2('W','D',input_buff+1,data_buff)) 
				return CMD_CWD;
			else if(_cmp3('D','U','P',input_buff+1,data_buff))
				return CMD_CDUP;
			return CMD_UNKNOWN;

		case 'D':
			if(_cmp3('E','L','E',input_buff+1,data_buff)) 
				return CMD_DELE;
			return CMD_UNKNOWN;
		case 'H':
			if(_cmp3('E','L','P',input_buff+1,data_buff)) 
				return CMD_HELP;
			return CMD_UNKNOWN;
		case 'L':
			if(_cmp3('I','S','T',input_buff+1,data_buff)) 
				return CMD_LIST;
			return CMD_UNKNOWN;
		case 'M':
			switch(input_buff[1]) {
				case 'K':
					if(_cmp1('D',input_buff+2,data_buff)) 
						return CMD_MKD;
				case 'O':
					if(_cmp2('D','E',input_buff+2,data_buff)) 
						return CMD_MODE;
				return CMD_UNKNOWN;
			}
		case 'N':
			switch(input_buff[1]) {
				case 'L':
					if(_cmp2('S','T',input_buff+2,data_buff)) 
						return CMD_NLST;
				case 'O':
					if(_cmp2('O','P',input_buff+2,data_buff)) 
						return CMD_NOOP;
				return CMD_UNKNOWN;
			}
		case 'P':
			switch(input_buff[1]) {
				case 'A':
					if(_cmp1('S',input_buff+2,data_buff))  {
						if(_cmp1('S',input_buff+3,data_buff)) 
							return CMD_PASS;
						if(_cmp1('V',input_buff+3,data_buff)) 
							return CMD_PASV;
						return CMD_UNKNOWN;
					}
				case 'O':
					if(_cmp2('R','T',input_buff+2,data_buff)) 
						return CMD_PORT;
				case 'W':
					if(_cmp1('D',input_buff+2,data_buff)) 
						return CMD_PWD;
				return CMD_UNKNOWN;
			}
		case 'Q':
			if(_cmp3('U','I','T',input_buff+1,data_buff)) 
					return CMD_QUIT;
				return CMD_UNKNOWN;
		case 'R':
			switch(input_buff[1]) {
				case 'E':
					switch(input_buff[2]) {
						case 'I':
							if(_cmp1('N',input_buff+3,data_buff)) 
								return CMD_REIN;
						case 'S':
							if(_cmp1('T',input_buff+3,data_buff)) 
								return CMD_REST;
						case 'T':
							if(_cmp1('R',input_buff+3,data_buff)) 
								return CMD_RETR;
						return CMD_UNKNOWN;
					}
					return CMD_UNKNOWN;
				case 'M':
					if(_cmp1('D',input_buff+2,data_buff)) 
						return CMD_RMD;
				case 'N':
					switch(input_buff[2]) {
						case 'F':
							if(_cmp1('R',input_buff+3,data_buff)) 
								return CMD_RNFR;
						case 'T':
							if(_cmp1('O',input_buff+3,data_buff)) 
								return CMD_RNTO;
						return CMD_UNKNOWN;
					}
				return CMD_UNKNOWN;
			}
		case 'S':
			switch(input_buff[1]) {
				case 'I':
					if(_cmp2('T','E',input_buff+2,data_buff)) 
						return CMD_SITE;
				case 'M':
					if(_cmp2('N','T',input_buff+2,data_buff)) 
						return CMD_SMNT;
				case 'T':
					switch(input_buff[2]) {
						case 'A':
							if(_cmp1('T',input_buff+3,data_buff)) 
								return CMD_STAT;
						case 'O':
							if(_cmp1('R',input_buff+3,data_buff)) 
								return CMD_STOR;
							if(_cmp1('U',input_buff+3,data_buff)) 
								return CMD_STOU;
						case 'R':
							if(_cmp1('U',input_buff+3,data_buff)) 
								return CMD_STRU;
					}
					return CMD_UNKNOWN;
				case 'Y':
					if(_cmp2('S','T',input_buff+2,data_buff)) 
						return CMD_SYST;
					return CMD_UNKNOWN;
			}
		case 'T':
			if(_cmp3('Y','P','E',input_buff+1,data_buff)) 
				return CMD_TYPE;
			return CMD_UNKNOWN;
		case 'U':
			if(_cmp3('S','E','R',input_buff+1,data_buff)) 
				return CMD_USER;
			return CMD_UNKNOWN;
		return CMD_UNKNOWN;
	}
	return CMD_UNKNOWN;
}
#ifdef __cplusplus
}
#endif

#endif /* _CMDPARSER_H */
