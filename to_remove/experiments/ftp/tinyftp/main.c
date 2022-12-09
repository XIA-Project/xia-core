/***************************************************************************
 *            main.c
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
#include "defines.h"
#include "fileutils.h"


/**
 * Read the command line options and if 
 * they are correct create a listenning server socket.
 * 
 */

int main(int argc,char *argv[])
{
	struct cmd_opts *copts= malloc(sizeof(struct cmd_opts));
	int result = pars_cmd_args(copts,argc,argv);
	
	switch(result) {
		case 0:
			return create_socket(copts);
		case -1:
			break;
		default:
			return 1;
	}
	return 0;
	
}
