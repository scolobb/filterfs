/*----------------------------------------------------------------------------*/
/*lib.h*/
/*----------------------------------------------------------------------------*/
/*Declarations of basic routines for filesystem manipulations*/
/*----------------------------------------------------------------------------*/
/*Copyright (C) 2001, 2002, 2005 Free Software Foundation, Inc.
  Written by Sergiu Ivanov <unlimitedscolobb@gmail.com>.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or * (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.*/
/*----------------------------------------------------------------------------*/
#ifndef __LIB_H__
#define __LIB_H__

/*----------------------------------------------------------------------------*/
#include <hurd.h>
#include <dirent.h>
#include <stddef.h>
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*Alignment of directory entries*/
#define DIRENT_ALIGN 4
/*----------------------------------------------------------------------------*/
/*The offset of the directory name in the directory entry structure*/
#define DIRENT_NAME_OFFS offsetof(struct dirent, d_name)
/*----------------------------------------------------------------------------*/
/*Computes the length of the structure before the name + the name + 0,
	all padded to DIRENT_ALIGN*/
#define DIRENT_LEN(name_len)\
	((DIRENT_NAME_OFFS + (name_len) + 1 + DIRENT_ALIGN - 1) &\
	~(DIRENT_ALIGN - 1))
/*----------------------------------------------------------------------------*/
/*Aligns the first parameter to the stride specified by the second parameter*/
#define ALIGN(x, stride) (((x) + (stride) - 1) & ~((stride) - 1))
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Fetches directory entries for `dir`*/
error_t
dir_entries_get
	(
	file_t dir,
	char ** dirent_data,					/*the list of directory entries as returned
																	by dir_readdir*/
	size_t * dirent_data_size,		/*the size of `dirent_data`*/
	struct dirent *** dirent_list /*the array of pointers to beginnings of
																	dirents in dirent_data*/
	);
/*----------------------------------------------------------------------------*/
#endif /*__LIB_H__*/
