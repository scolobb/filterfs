/*----------------------------------------------------------------------------*/
/*node.c*/
/*----------------------------------------------------------------------------*/
/*Implementation of node management strategies*/
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

/*----------------------------------------------------------------------------*/
#define _GNU_SOURCE 1
/*----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#include "debug.h"
#include "node.h"
#include "options.h"
#include "lib.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The lock protecting the underlying filesystem*/
struct mutex ulfs_lock = MUTEX_INITIALIZER;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Derives a new node from `lnode` and adds a reference to `lnode`*/
error_t
node_create
	(
	lnode_t * lnode,
	node_t ** node	/*store the result here*/
	)
	{
	error_t err = 0;

	/*Create a new netnode*/
	netnode_t * netnode_new = malloc(sizeof(netnode_t));
	
	/*If the memory could not be allocated*/
	if(netnode_new == NULL)
		err = ENOMEM;
	else
		{
		/*create a new node from the netnode*/
		node_t * node_new = netfs_make_node(netnode_new);
		
		/*If the creation failed*/
		if(node_new == NULL)
			{
			/*set the error code*/
			err = ENOMEM;

			/*destroy the netnode created above*/
			free(netnode_new);
			
			/*stop*/
			return err;
			}
			
		/*link the lnode to the new node*/
		lnode->node = node_new;
		lnode_ref_add(lnode);
		
		/*setup the references in the newly created node*/
		node_new->nn->lnode = lnode;
		node_new->nn->flags = 0;
		node_new->nn->ncache_next = node_new->nn->ncache_prev = NULL;
		
		/*store the result of creation in the second parameter*/
		*node = node_new;
		}
		
	/*Return the result of operations*/
	return err;
	}/*node_create*/
/*----------------------------------------------------------------------------*/
/*Creates the root node and the corresponding lnode*/
error_t
node_create_root
	(
	node_t ** root_node	/*store the result here*/
	)
	{
	/*Try to create a new lnode*/
	lnode_t * lnode;
	error_t err = lnode_create(NULL, &lnode);
	
	/*Stop, if the creation failed*/
	if(err)
		return err;
		
	/*Try to derive the node corresponding to `lnode`*/
	node_t * node;
	err = node_create(lnode, &node);
	
	/*If the operation failed*/
	if(err)
		{
		/*destroy the created lnode*/
		lnode_destroy(lnode);
		
		/*stop*/
		return err;
		}
		
	/*Release the lock on the lnode*/
	mutex_unlock(&lnode->lock);
	
	/*Store the result in the parameter*/
	*root_node = node;
	
	/*Return the result*/
	return err;
	}/*node_create_root*/
/*----------------------------------------------------------------------------*/
/*Initializes the port to the underlying filesystem for the root node*/
error_t
node_init_root
	(
	node_t * node	/*the root node*/
	)
	{
	error_t err = 0;

	/*Acquire a lock for operations on the underlying filesystem*/
	mutex_lock(&ulfs_lock);
	
	/*Open the port to the directory specified in `dir`*/
	node->nn->port = file_name_lookup(dir, O_READ | O_DIRECTORY, 0);
	
	/*If the directory could not be opened*/
	if(node->nn->port == MACH_PORT_NULL)
		{
		/*set the error code accordingly*/
		err = errno;
		LOG_MSG("node_init_root: Could not open the port for %s.", dir);
		}
	LOG_MSG("node_init_root: Port for %s opened successfully.", dir);
	LOG_MSG("\tPort: 0x%lX", (unsigned long)node->nn->port);
	
	/*Release the lock for operations on the undelying filesystem*/
	mutex_unlock(&ulfs_lock);
	
	/*Return the result of operations*/
	return err;
	}/*node_init_root*/
/*----------------------------------------------------------------------------*/
/*Frees a list of dirents*/
void
node_entries_free
	(
	node_dirent_t * dirents	/*free this*/
	)
	{
	/*The current and the next elements*/
	node_dirent_t * dirent, * dirent_next;
	
	/*Go through all elements of the list*/
	for(dirent = dirents; dirent; dirent = dirent_next)
		{
		/*store the next element*/
		dirent_next = dirent->next;
		
		/*free the dirent stored in the current element of the list*/
		free(dirent->dirent);
		
		/*free the current element*/
		free(dirent);
		}
	}/*node_entries_free*/
/*----------------------------------------------------------------------------*/
/*Reads the directory entries from `node`, which must be locked*/
error_t
node_entries_get
	(
	node_t * node,
	node_dirent_t ** dirents /*store the result here*/
	)
	{
	error_t err = 0;

	/*The list of dirents*/
	struct dirent ** dirent_list, **dirent;
	
	/*The head of the list of dirents*/
	node_dirent_t * node_dirent_list = NULL;
	
	/*The size of the array of pointers to dirent*/
	size_t dirent_data_size;
	
	/*The array of dirents*/
	char * dirent_data;

	/*Obtain the directory entries for the given node*/
	err = dir_entries_get
		(node->nn->port, &dirent_data, &dirent_data_size, &dirent_list);
	if(err)
		return err;
		
	/*The new entry in the list*/
	node_dirent_t * node_dirent_new;
	
	/*The new dirent*/
	struct dirent * dirent_new;
	
	LOG_MSG("node_entries_get: Getting entries for %p", node);
	LOG_MSG("\tsizeof(dirent) = %d", sizeof(*dirent_new));
	LOG_MSG("\tsizeof(dirent::d_ino)    = %d", sizeof(dirent_new->d_ino));
	LOG_MSG("\tsizeof(dirent::d_type)   = %d", sizeof(dirent_new->d_type));
	LOG_MSG("\tsizeof(dirent::d_reclen) = %d", sizeof(dirent_new->d_reclen));
	LOG_MSG("\tsizeof(dirent::d_namlen) = %d", sizeof(dirent_new->d_namlen));
	LOG_MSG("\tsizeof(dirent::d_name)   = %d", sizeof(dirent_new->d_name));

	/*The name of the current dirent*/
	char * name;

	/*The length of the current name*/
	int name_len;
	
	/*The size of the current dirent*/
	int size;
	
	/*Go through all elements of the list of pointers to dirent*/
	for(dirent = dirent_list; *dirent; ++dirent)
		{
		/*obtain the name of the current dirent*/
		name = (char *)(*dirent + 1);
		
		/*If the current dirent is either '.' or '..', skip it*/
		if((strcmp(name, ".") == 0) ||	(strcmp(name, "..") == 0))
			continue;
		
		/*obtain the length of the current name*/
		name_len = strlen(name);
		
		/*obtain the length of the current dirent*/
		size = DIRENT_LEN(name_len);
		
		/*create a new list element*/
		node_dirent_new = malloc(sizeof(node_dirent_t));
		if(!node_dirent_new)
			{
			err = ENOMEM;
			break;
			}
			
		/*create a new dirent*/
		dirent_new = malloc(size);
		if(!dirent_new)
			{
			free(node_dirent_new);
			err = ENOMEM;
			break;
			}
			
		/*fill the dirent with information*/
		dirent_new->d_ino			= (*dirent)->d_ino;
		dirent_new->d_type 		= (*dirent)->d_type;
		dirent_new->d_reclen	= size;
		strcpy((char *)dirent_new + DIRENT_NAME_OFFS, name);
		
		/*add the dirent to the list*/
		node_dirent_new->dirent = dirent_new;
		node_dirent_new->next = node_dirent_list;
		node_dirent_list = node_dirent_new;
		}
	
	/*If something went wrong in the loop*/
	if(err)
		/*free the list of dirents*/
		node_entries_free(node_dirent_list);
	else
		/*store the list of dirents in the second parameter*/
		*dirents = node_dirent_list;
	
	/*Free the list of pointers to dirent*/
	free(dirent_list);
	
	/*Free the results of listing the dirents*/
	munmap(dirent_data, dirent_data_size);
	
	/*Return the result of operations*/
	return err;
	}/*node_entries_get*/
/*----------------------------------------------------------------------------*/

