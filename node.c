/*----------------------------------------------------------------------------*/
/*node.c*/
/*----------------------------------------------------------------------------*/
/*Implementation of node management strategies*/
/*----------------------------------------------------------------------------*/
/*Based on the code of unionfs translator.*/
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
#include "filterfs.h"
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
/*Destroys the specified node and removes a light reference from the
	associated light node*/
void
node_destroy
	(
	node_t * np
	)
	{
	/*Die if the node does not belong to node cache*/
	assert(!np->nn->ncache_next || !np->nn->ncache_prev);
	
	/*Destroy the port to the underlying filesystem allocated to the node*/
	PORT_DEALLOC(np->nn->port);
	
	/*Lock the lnode corresponding to the current node*/
	mutex_lock(&np->nn->lnode->lock);
	
	/*Orphan the light node*/
	np->nn->lnode->node = NULL;
	
	/*Remove a reference from the lnode*/
	lnode_ref_remove(np->nn->lnode);
	
	/*Free the netnode and the node itself*/
	free(np->nn);
	free(np);
	}/*node_destroy*/
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
		
		/*release the lock and stop*/
		mutex_unlock(&ulfs_lock);
		return err;
		}

	LOG_MSG("node_init_root: Port for %s opened successfully.", dir);
	LOG_MSG("\tPort: 0x%lX", (unsigned long)node->nn->port);
	
	/*Stat the root node*/
	err = io_stat(node->nn->port, &node->nn_stat);
	if(err)
		{
		/*deallocate the port*/
		PORT_DEALLOC(node->nn->port);
		
		LOG_MSG("node_init_root: Could not stat the root node.");
		
		/*unlock the mutex and exit*/
		mutex_unlock(&ulfs_lock);
		return err;
		}
	
	/*Set the path to the corresponding lnode to `dir`*/
	node->nn->lnode->path = strdup(dir);
	if(!node->nn->lnode->path)
		{
		/*deallocate the port*/
		PORT_DEALLOC(node->nn->port);

		/*unlock the mutex*/
		mutex_unlock(&ulfs_lock);

		LOG_MSG("node_init_root: Could not strdup the directory.");
		return ENOMEM;
		}
	
	/*The current position in dir*/
	char * p = dir + strlen(dir);
	
	/*Go through the path from end to beginning*/
	for(; p >= dir; --p)
		{
		/*If the current character is a '/'*/
		if(*p == '/')
			{
			/*If p is not the first character*/
			if(p > dir)
				{
				/*if this slash is escaped, skip it*/
				if(*(p - 1) == '\\')
					continue;
				}
			
			/*advance the pointer to the first character after the slash*/
			++p;

			/*stop*/
			break;
			}
		}
		
	LOG_MSG("node_init_root: The name of root node is %s.", p);
	
	/*Set the name of the lnode to the last element in the path to dir*/
	node->nn->lnode->name = strdup(p);
	/*If the name of the node could not be duplicated*/
	if(!node->nn->lnode->name)
		{
		/*free the name of the path to the node and deallocate teh port*/
		free(node->nn->lnode->path);
		PORT_DEALLOC(node->nn->port);
		
		/*unlock the mutex*/
		mutex_unlock(&ulfs_lock);

		LOG_MSG("node_init_root: Could not strdup the name of the root node.");
		return ENOMEM;
		}
	
	/*Compute the length of the name of the root node*/
	node->nn->lnode->name_len = strlen(p);

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

	/*Obtain the path to the current node*/
	char * path_to_node = node->nn->lnode->path;
	
	/*The number of PROPERTY_PARAMs in the property*/
	int param_entries_count = 0;
	
	/*The length of the property*/
	size_t property_len = (property) ? (strlen(property)) : (0);
	
	/*The length of PROPERTY_PARAM*/
	size_t property_param_len = strlen(PROPERTY_PARAM);

	/*The full name and the filtering command*/
	char * full_name = NULL, * cmd = NULL;

	/*The lengths of the full name and the filtering command in chunks*/
	size_t full_name_size = 1, cmd_size = 1;

	/*If some property was indeed specified*/
	if(property_len != 0)
		{
		/*the pointer to the current occurrence of PROPERTY_PARAM*/
		char * occurrence = strstr(property, PROPERTY_PARAM);
		
		/*count the number of occurrences*/
		for(; occurrence;
			occurrence = strstr(occurrence + 1, PROPERTY_PARAM),
			++param_entries_count);

		/*try allocate the memory for the fullname and the filtering command*/
		full_name = malloc(full_name_size * STRING_CHUNK);
		if(!full_name)
			return ENOMEM;

		cmd = malloc(cmd_size * STRING_CHUNK);
		if(!cmd)
			{
			free(full_name);
			return ENOMEM;
			}
		}

	/*Obtain the length of the path*/
	size_t pathlen = strlen(path_to_node);

	/*Checks if the given file satisfies the property. Zero value means that
		the entry must be filtered out*/
	int
	check_property
		(
		const char * name	/*the name of the file*/
		)
		{
		/*If there is no property*/
		if(!property)
			/*no filtering will be applied, any name is OK*/
			return 0;
		
		/*Everything OK at first*/
		err = 0;
		
		/*Compute the length of the full name once*/
		size_t full_name_len = pathlen + 1 + strlen(name) + 1;

		/*See how much space (in chunks) is required for the full name*/
		int chunks = full_name_size;
		for(; full_name_len > chunks * STRING_CHUNK; ++chunks);
		
		/*If more memory is requied*/
		if(chunks > full_name_size)
			{
			/*free the old full name*/
			free(full_name);

			/*try to allocate the new memory*/
			full_name = malloc(chunks * STRING_CHUNK);
			if(!full_name)
				{
				err = ENOMEM;
				free(cmd); /*the string for the command is definitely allocated here*/
				return 0;
				}
			
			/*store the new size*/
			full_name_size = chunks;
			}
		
		/*Initialize `full_name` as a valid string*/
		full_name[0] = 0;
		
		/*Construct the full name*/
		strcpy(full_name, path_to_node);
		strcat(full_name, "/");
		strcat(full_name, name);
		
		/*LOG_MSG("node_entries_get: Applying filter to %s...", full_name);*/
		
		/*Compute the space required for the final filtering command*/
		size_t sz = property_len + (strlen(full_name) - property_param_len)
			* param_entries_count;
		
		/*See how much space (in chunks) is required for the command*/
		for(chunks = cmd_size; sz > chunks * STRING_CHUNK; ++chunks);
		
		/*If more memory is requied*/
		if(chunks > cmd_size)
			{
			/*free the old command*/
			free(cmd);

			/*try to allocate the new memory*/
			cmd = malloc(chunks * STRING_CHUNK);
			if(!cmd)
				{
				err = ENOMEM;
				free(full_name);	/*the string for the full name is
														definitely allocated here*/
				return 0;
				}
			
			/*store the new size*/
			cmd_size = chunks;
			}

		/*Initialize `cmd` as a valid string*/
		cmd[0] = 0;
			
		/*The current occurence of PROPERTY_PARAM in property*/
		char * p = strstr(property, PROPERTY_PARAM);
		
		/*The pointer to the current position in the property*/
		char * propp = property;
		
		/*While the command has not been constructed*/
		for(; p; p = strstr(propp, PROPERTY_PARAM))
			{
			/*add the new part of the property to the command*/
			strncat(cmd, propp, p - propp);
			
			/*add the filename to the command*/
			strcat(cmd, full_name);
			
			/*LOG_MSG("\tcmd = '%s'", cmd);*/

			/*advance the pointer in the property*/
			propp = p + property_param_len;

			/*LOG_MSG("\tpropp points at '%s'", propp);*/
			}
		
		/*Copy the rest of the property to the command*/
		strcat(cmd, propp);
		
		/*LOG_MSG("node_entries_get: The filtering command: '%s'.", cmd);*/

		/*Execute the command*/
		int xcode = WEXITSTATUS(system(cmd));
		
		/*Return the exit code of the command*/
		return xcode;
		}/*check_property*/

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
		{
		return err;
		}
		
	/*The new entry in the list*/
	node_dirent_t * node_dirent_new;
	
	/*The new dirent*/
	struct dirent * dirent_new;
	
	/*LOG_MSG("node_entries_get: Getting entries for %p", node);*/

	/*The name of the current dirent*/
	char * name;

	/*The length of the current name*/
	size_t name_len;
	
	/*The size of the current dirent*/
	size_t size;
	
	/*The exit code of property*/
	int good;
	
	/*Go through all elements of the list of pointers to dirent*/
	for(dirent = dirent_list; *dirent; ++dirent)
		{
		/*obtain the name of the current dirent*/
		name = &((*dirent)->d_name[0]);
		
		/*If the current dirent is either '.' or '..', skip it*/
		if((strcmp(name, ".") == 0) ||	(strcmp(name, "..") == 0))
			continue;
		
		/*check if the current dirent has the property*/
		good = check_property(name);
		if(err)
			break;
 		
		/*If the current entry is not good, skip it*/
		if(good != 0)
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

	/*Free the full name and the command (if these are present at all)*/
	if(full_name)
		free(full_name);
	if(cmd)
		free(cmd);
	
	/*Return the result of operations*/
	return err;
	}/*node_entries_get*/
/*----------------------------------------------------------------------------*/
/*Looks up `name` under 'dir' with 'flags' as open flags; return the first
	successfully looked up port in `port` and the corresponding stat information
	in `stat`*/
error_t
node_lookup_file
	(
	node_t * dir,
	char * name,
	int flags,
	file_t * port,
	io_statbuf_t * s
	)
	{
	error_t err = ENOENT;
	
	/*The port to the looked up file*/
	file_t p;

	/*The stat information about the file*/
	io_statbuf_t stat;
	
	/*If the port for `dir` is invalid, return the proper error*/
	if(dir->nn->port == MACH_PORT_NULL)
		return ENOENT;
		
	/*Try to lookup the specified `name`*/
	err = file_lookup
		(
		dir->nn->port,
		name,
		flags,
		0,	/*fall back to default flags (?)*/
		0,
		&p,
		&stat
		);
	if(err)
		return err;
	
	/*If we are asked to lookup the root node of filterfs*/
	if
		(
		(stat.st_ino == underlying_node_stat.st_ino)
		&& (stat.st_fsid == underlying_node_stat.st_fsid)
		)
		/*remark the fact that the requested node is our root*/
		err = ELOOP;
	
	/*If no errors have been encountered*/
	if(!err)
		{
		/*put the port and the stat information into the parameters*/
		*port = p;
		*s = stat;
		}
	
	/*Return the result of performing operations*/
	return err;
	}/*node_lookup_file*/
/*----------------------------------------------------------------------------*/
/*Makes sure that all ports to the underlying filesystem of `node` are up to
	date*/
error_t
node_update
	(
	node_t * node
	)
	{
	error_t err = 0;

	/*The full path to this node*/
	char * path;
	
	/*Stat information for `node`*/
	io_statbuf_t stat;
	
	/*The port to the file corresponding to `node`*/
	file_t port;

	/*If the specified node is the root node or if it must not be updated*/
	if(NODE_IS_ROOT(node) || (node->nn->flags & FLAG_NODE_ULFS_FIXED))
		/*do nothing*/
		return err; /*return 0; actually*/
		
	/*Gain exclusive access to the root node of the filesystem*/
	mutex_lock(&netfs_root_node->lock);
	
	/*Construct the full path to `node`*/
	err = lnode_path_construct(node->nn->lnode, &path);
	if(err)
		{
		mutex_unlock(&netfs_root_node->lock);
		return err;
		}
		
	/*Deallocate `node`'s port to the underlying filesystem*/
	if(node->nn->port)
		PORT_DEALLOC(node->nn->port);
		
	/*Try to lookup the file for `node` in its untranslated version*/
	err = file_lookup
		(
		netfs_root_node->nn->port, path, O_READ | O_NOTRANS, O_NOTRANS,
		0, &port, &stat
		);
	if(err)
		{
		node->nn->port = MACH_PORT_NULL;
		err = 0; /*failure (?)*/
		return err;
		}
	
	/*If the node looked up is actually the root node of filterfs filesystem*/
	if
		(
		(stat.st_ino == underlying_node_stat.st_ino)
		&& (stat.st_fsid == underlying_node_stat.st_fsid)
		)
		/*set `err` accordingly*/
		err = ELOOP;
	else
		{
		/*deallocate the obtained port*/
		PORT_DEALLOC(port);
		
		/*obtain the translated version of the required node*/
		err = file_lookup
			(netfs_root_node->nn->port, path, O_READ, 0, 0, &port, &stat);
		}
		
	/*If there have been errors*/
	if(err)
		/*reset the port*/
		port = MACH_PORT_NULL;
		
	/*Store the port in the node*/
	node->nn->port = port;
	
	/*Remove the flag about the invalidity of the current node and set the
		flag that the node is up-to-date*/
	node->nn->flags &= ~FLAG_NODE_INVALIDATE;
	node->nn->flags |= FLAG_NODE_ULFS_UPTODATE;
	
	/*Release the lock on the root node of filterfs filesystem*/
	mutex_unlock(&netfs_root_node->lock);
	
	/*Return the result of operations*/
	return err;
	}/*node_update*/
/*----------------------------------------------------------------------------*/
/*Computes the size of the given directory*/
error_t
node_get_size
	(
	node_t * dir,
	OFFSET_T * off
	)
	{
	error_t err = 0;

	/*The final size*/
	size_t size = 0;
	
	/*The number of directory entries*/
	/*int count = 0;*/
	
	/*The the node in the directory entries list from which we start counting*/
	/*node_dirent_t * dirent_start = NULL;*/
	
	/*The currently analyzed dirent*/
	node_dirent_t * dirent_current = NULL;
	
	/*The pointer to the beginning of the list of dirents*/
	node_dirent_t * dirent_list = NULL;
	
	/*The first entry we have to analyze*/
	/*int first_entry = 2;*/
	
	/*Takes into consideration the name of the current dirent*/
	void
	bump_size
		(
		const char * name
		)
		{
		/*Increment the current size by the size of the current dirent*/
		size += DIRENT_LEN(strlen(name));
		
		/*Count the current dirent*/
		/*++count;*/
		}/*bump_size*/
		
	/*Obtain the list of entries in the current directory*/
	err = node_entries_get(dir, &dirent_list);
	if(err)
		return err;
	
	/*Obtain the pointer to the dirent which has the number first_entry*/
	/*Actually, the first element of the list*/
	/*This code is included in unionfs, but it's completely useless here*/
	/*for
		(
		dirent_start = dirent_list, count = 2;
		dirent_start && count < first_entry;
		dirent_start = dirent_start->next, ++count
		);*/
	
	/*Reset the count*/
	/*count = 0;*/
	
	/*Make space for '.' and '..' entries*/
	/*This code is included in unionfs, but it's completely useless here*/
	/*if(first_entry == 0)
		bump_size(".");
	if(first_entry <= 1)
		bump_size("..");*/
	
	/*See how much space is required for the node*/
	for
		(
		dirent_current = dirent_list/*dirent_start*/; dirent_current;
		dirent_current = dirent_current->next
		)
		bump_size(dirent_current->dirent->d_name);
		
	/*Free the list of dirents*/
	node_entries_free(dirent_list);
	
	/*Return the size*/
	*off = size;
	return 0;
	}/*node_get_size*/
/*----------------------------------------------------------------------------*/
/*Remove the file called `name` under `dir`*/
error_t
node_unlink_file
	(
	node_t * dir,
	char * name
	)
	{
	error_t err = 0;

	/*The port to the file which will be unlinked*/
	mach_port_t p;
	
	/*Stat information about the file which will be unlinked*/
	io_statbuf_t stat;
	
	/*If port corresponding to `dir` is invalid*/
	if(dir->nn->port == MACH_PORT_NULL)
		/*stop with an error*/
		return ENOENT; /*FIXME: Is the return value indeed meaningful here?*/
	
	/*Attempt to lookup the specified file*/
	err = file_lookup(dir->nn->port, name, O_NOTRANS, O_NOTRANS, 0, &p, &stat);
	if(err)
		return err;
	
	/*Deallocate the obtained port*/
	PORT_DEALLOC(p);
	
	/*Unlink file `name` under `dir`*/
	err = dir_unlink(dir->nn->port, name);
	if(err)
		return err;
	
	return err;
	}/*node_unlink_file*/
/*----------------------------------------------------------------------------*/
