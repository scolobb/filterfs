/*----------------------------------------------------------------------------*/
/*lib.h*/
/*----------------------------------------------------------------------------*/
/*Basic routines for filesystem manipulations*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#define _GNU_SOURCE 1
/*----------------------------------------------------------------------------*/
#include "lib.h"
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
	)
	{
	error_t err;
	
	/*The data returned by dir_readdir*/
	char * data;
	
	/*The size of `data`*/
	size_t data_size;
	
	/*The number of entries in `data`*/
	int entries_num;
	
	/*Try to read the contents of the specified directory*/
	err = dir_readdir(dir, &data, &data_size, 0, -1, 0, &entries_num);
	if(err)
		return err;
		
	/*Create a new list of dirents*/
	struct dirent ** list;
	
	/*Allocate the memory for the list of dirents and for the
		finalizing element*/
	list = malloc(sizeof(struct dirent *) * (entries_num + 1));
	
	/*If memory allocation has failed*/
	if(!list)
		{
		/*free the result of dir_readdir*/
		munmap(data, data_size);
		
		/*return the corresponding error*/
		return err;
		}
	}/*dir_entries_get*/
/*----------------------------------------------------------------------------*/
