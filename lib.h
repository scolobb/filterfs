/*----------------------------------------------------------------------------*/
/*lib.h*/
/*----------------------------------------------------------------------------*/
/*Declarations of basic routines for filesystem manipulations*/
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
	struct dirent *** dirent_list /*the array of dirents*/
	);
/*----------------------------------------------------------------------------*/
#endif
