/***************************************************************************
 *            fileutils.h
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

#ifndef _FILEUTILS_H
#define _FILEUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif
	bool write_list(int , int , const char *);
	bool stat_file(int, const char *,char *);
	bool change_dir(int ,const char *,char *,char *,char *);
	bool retrieve_file(int, int, int, const char *);
	bool store_file(int, int, int, const char *);
	bool stou_file(int , int, int, int);
	bool make_dir(int,const char*,char *);
	bool remove_dir(int ,const char *);
	bool delete_file(int ,const char *);
	bool rename_fr(int ,const char *,const char *);
#ifdef __cplusplus
}
#endif

#endif /* _FILEUTILS_H */
