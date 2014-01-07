/***************************************************************************
 *            opts.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <error.h>
#include <errno.h>
#include "defines.h"

int assign_option(int, const char*,struct cmd_opts *,int);
static void print_help();
int dir_exists(const char *); 

/**
 * pars_cmd_args:
 * Parse command line arguments and create the structure #copts with appropriate settings.
 */
int pars_cmd_args(struct cmd_opts *copts,int argc,char *argv[]) {
	int len,i;
	if(argc<2) {
		printf("    USAGE: tinyftp [OPTIONS]\n    Run tinyftp -h for help.\n");
		return 1;
	}
	copts->daemonize    = FALSE;
	copts->listen_any   = TRUE;
	copts->port         = 21;
	copts->userid       = 0;
	copts->chrootdir    = "./";
	copts->max_conn     = 5;
	copts->listen_addr  = NULL;
	
	// remember the current read options. e.g. if the last read argument was "-l"
	// current option is "limit", so we are waiting for a number of concurrent connections.
	int current_option=0; 
	for(i=1;i<argc;i++) {
		len = strlen(argv[i]);
		if(len <1)
			continue;
		if(current_option==0) {
			if(argv[i][0]=='-') {
				
				switch(argv[i][1]) {
					
					case 'l': // limit
						if(len>2) {
							assign_option('l', argv[i]+2, copts,len-2);
							current_option =0;
						} else {
							current_option = 'l';
						}
					break;
					case 'u': //run as user id
						if(len>2) {
							assign_option('p', argv[i]+2, copts,len-2);
						} else {
							current_option = 'u';
						}
					break;
					case 'p': //port
						if(len>2) {
							assign_option('p', argv[i]+2, copts,len-2);
						} else {
							current_option =  'p';
						}
					break;
					case 's': //server address
						if(len>2) {
							assign_option('s', argv[i]+2, copts,len-2);
						} else {
							current_option =  's';
						}
					break;
					case 'c': //chto dir, default ./
						if(len>2) {
							assign_option('c', argv[i]+2, copts,len-2);
						} else {
							current_option = 'c';
						}
					break;
					case 'd':
						copts->daemonize = TRUE;
						break;
					
					case 'h':
					default:
						print_help();
						return -1;
				}
			}
		}
		else {
			if(assign_option(current_option,argv[i],copts,len)!=0) {
				return 1;
			}
			current_option =  0;
		}
	}
	return 0;
}

/**
 * Write a single option in the struct #copts.
 */
int assign_option(current_option, arg,copts,len)
	int current_option;
	const char* arg;
	struct cmd_opts *copts;
	int len;
		{
	int p;
	switch(current_option) {
		case 's': // server address
			if(arg[0]=='0' || arg[0]=='*' || strcmp(arg,"0.0.0.0")==0) {
				copts->listen_any = TRUE;
				copts->listen_addr = NULL;
			} else {
				copts->listen_any = FALSE;
				copts->listen_addr = (char *)arg;
			}
			break;
		case 'p': // server port
			p = toint(arg,TRUE);
			if (p<1 || p > 32768) {
				return 2;
			}
			copts->port = p;
		break;
		case 'l': // limit of the concurrent connections
			p = toint(arg,TRUE);
			if (p<1 || p > 600) {
				printf("Limit should be numeric, in range 1, 600");
				return 3;
			}
			copts->max_conn = p;
		break;
		case 'u': // run as userid
			p = toint(arg,TRUE);
			if (p<0 || p > 99999) {
				printf("User id should be numeric, in range 0, 99999");
				return 4;
			}
			copts->userid = p;
		break;
		case 'c': // current working directory
			if(dir_exists(arg))
				return 5;
			copts->chrootdir = malloc(sizeof(char *)*len);
			strcpy(copts->chrootdir,arg);
		break;
	}
	return 0;
}

/**
 * dir_exists:
 * Check if directory #path exists. On succes return 0
 */
int dir_exists(const char *path) {
	DIR *dir =opendir(path) ;
	if(dir==NULL) {
		printf("Error openning directory \"%s\", error was:\n  ",path);
		switch(errno) {
			case EACCES:
				printf("Access denied.\n");
				closedir(dir);
				return -1;
			case EMFILE:
				printf("Too many file descriptors in use by process.\n");
				closedir(dir);
				return -1;
			case ENFILE:
				printf("Too many files are currently open in the system.\n");
				closedir(dir);
				break;
			case ENOENT:
				printf("Directory does not exist, or is an empty string.\n");
				closedir(dir);
				return -1;
			case ENOMEM:
				printf("Insufficient memory to complete the operation..\n");
				closedir(dir);
				return -1;
			default:
			case ENOTDIR:
				printf("\"%s\" is not a directory.\n",path);
				closedir(dir);
				return -1;
		}
	}
	closedir(dir);
	return 0;
}

/**
 * Read a string and return its representation as an integer.
 */
int toint(str,strict) 
	const char* str ;
	bool strict;
	{
	int len = strlen(str);
	if(len<1) {
		return -1;
	}
	int i;
	int base=1;
	int retval = 0;
	for(i=len-1;i>=0;i--,base *= 10) {
		if(base>=10000) {
			return -1;
		}
		if((int)str[i] >47 && (int)str[i]<58) {
			retval+=base*(str[i]-48);
		} else {
			if(strict)
				return -1;
		}
	}
	return retval;
}

/**
 * Print out to the stdin a help message with a short explanation
 * of the available command line options and usage.
 */
void print_help() {
	printf("Usage: tinyftp [OPTIONS]\n");
	printf(" -h,\n     Display tinyftp help\n");
	printf(" -d,\n     Daemonize after startup\n");
	printf(" -s [address],\n     Set the server address\n");
	printf(" -p [port],\n     Set the server port \n");
	printf(" -l [limit],\n     Limit to \"[limit]\" concurrent connections\n");
	printf(" -u [userid],\n     Do filesystem operations as \"[userid]\"\n");
	printf(" -c [directory],\n     Default directory \n");
}
