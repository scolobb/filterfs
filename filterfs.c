/*----------------------------------------------------------------------------*/
/*filter.c*/
/*----------------------------------------------------------------------------*/
/*The filtering translator*/
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
#include "filterfs.h"
/*----------------------------------------------------------------------------*/
#include <error.h>
#include <argp.h>
#include <argz.h>
#include <hurd/netfs.h>
#include <fcntl.h>
/*----------------------------------------------------------------------------*/
#include "debug.h"
#include "options.h"
#include "ncache.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*The state modes use in open*/
#define OPENONLY_STATE_MODES (O_CREAT | O_EXCL | O_NOLINK | O_NOTRANS \
	| O_NONBLOCK)
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The name of the server*/
char * netfs_server_name = "filterfs";
/*----------------------------------------------------------------------------*/
/*The version of the server*/
char * netfs_server_version = "0.0";
/*----------------------------------------------------------------------------*/
/*The maximal length of a chain of symbolic links*/
int netfs_maxsymlinks = 12;
/*----------------------------------------------------------------------------*/
/*A port to the underlying node*/
mach_port_t underlying_node;
/*----------------------------------------------------------------------------*/
/*Status information for the underlying node*/
io_statbuf_t underlying_node_stat;
/*----------------------------------------------------------------------------*/
/*Mapped time used for updating node information*/
volatile struct mapped_time_value * maptime;
/*----------------------------------------------------------------------------*/
/*The filesystem ID*/
pid_t fsid;
/*----------------------------------------------------------------------------*/
/*The file to print debug messages to*/
FILE * filterfs_dbg;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Attempts to create a file named `name` in `dir` for `user` with mode `mode`*/
error_t
netfs_attempt_create_file
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	mode_t mode,
	struct node ** node
	)
	{
	LOG_MSG("netfs_attempt_create_file");

	/*Unlock `dir` and say that we can do nothing else here*/
	mutex_unlock(&dir->lock);
	return EOPNOTSUPP;
	}/*netfs_attempt_create_file*/
/*----------------------------------------------------------------------------*/
/*Return an error if the process of opening a file should not be allowed
	to complete because of insufficient permissions*/
error_t
netfs_check_open_permissions
	(
	struct iouser * user,
	struct node * np,
	int flags,
	int newnode
	)
	{
	LOG_MSG("netfs_check_open_permissions: '%s'", np->nn->lnode->name);

	error_t err = 0;
	
	/*Cheks user's permissions*/
	if(flags & O_READ)
		err = fshelp_access(&np->nn_stat, S_IREAD, user);
	if(!err && (flags & O_WRITE))
		err = fshelp_access(&np->nn_stat, S_IWRITE, user);
	if(!err && (flags & O_EXEC))
		err = fshelp_access(&np->nn_stat, S_IEXEC, user);
		
	/*Return the result of the check*/
	return err;
	}/*netfs_check_open_permissions*/
/*----------------------------------------------------------------------------*/
/*Attempts an utimes call for the user `cred` on node `node`*/
error_t
netfs_attempt_utimes
	(
	struct iouser * cred,
	struct node * node,
	struct timespec * atime,
	struct timespec * mtime
	)
	{
	LOG_MSG("netfs_attempt_utimes");

	error_t err = 0;
	
	/*See what information is to be updated*/
	int flags = TOUCH_CTIME;
	
	/*Check if the user is indeed the owner of the node*/
	err = fshelp_isowner(&node->nn_stat, cred);
	
	/*If the user is allowed to do utimes*/
	if(!err)
		{
		/*If atime is to be updated*/
		if(atime)
			/*update the atime*/
			node->nn_stat.st_atim = *atime;
		else
			/*the current time will be set as the atime*/
			flags |= TOUCH_ATIME;
		
		/*If mtime is to be updated*/
		if(mtime)
			/*update the mtime*/
			node->nn_stat.st_mtim = *mtime;
		else
			/*the current time will be set as mtime*/
			flags |= TOUCH_MTIME;
		
		/*touch the file*/
		fshelp_touch(&node->nn_stat, flags, maptime);
		}
	
	/*Return the result of operations*/
	return err;
	}/*netfs_attempt_utimes*/
/*----------------------------------------------------------------------------*/
/*Returns the valid access types for file `node` and user `cred`*/
error_t
netfs_report_access
	(
	struct iouser * cred,
	struct node * np,
	int * types
	)
	{
	LOG_MSG("netfs_report_access");

	/*No access at first*/
	*types = 0;
	
	/*Check the access and set the required bits*/
	if(fshelp_access(&np->nn_stat, S_IREAD,  cred) == 0)
		*types |= O_READ;
	if(fshelp_access(&np->nn_stat, S_IWRITE, cred) == 0)
		*types |= O_WRITE;
	if(fshelp_access(&np->nn_stat, S_IEXEC,  cred) == 0)
		*types |= O_EXEC;

	/*Everything OK*/
	return 0;
	}/*netfs_report_access*/
/*----------------------------------------------------------------------------*/
/*Validates the stat data for the node*/
error_t
netfs_validate_stat
	(
	struct node * np,
	struct iouser * cred
	)
	{
	LOG_MSG("netfs_validate_stat: '%s'", np->nn->lnode->name);

	error_t err = 0;
	
	/*If we are not at the root*/
	if(np != netfs_root_node)
		{
		/*If the node is not surely up-to-date*/
		if(!(np->nn->flags & FLAG_NODE_ULFS_UPTODATE))
			{
			/*update it*/
			err = node_update(np);
			}
		
		/*If no errors have yet occurred*/
		if(!err)
			{
			/*If the port to the file corresponding to `np` is valid*/
			if(np->nn->port != MACH_PORT_NULL)
				{
				/*We have a directory here (normally, only they maintain an open port).
					Generally, our only concern is to maintain an open port in this case*/
				
				/*attempt to stat this file*/
				err = io_stat(np->nn->port, &np->nn_stat);

				/*If stat information has been successfully obtained for the file*/
				if(!err)
					/*duplicate the st_mode field of stat structure*/
					np->nn_translated = np->nn_stat.st_mode;
				}
			else
				{
				/*We, most probably, have something which is not a directory. Therefore
					we will open the port and close it after the stat, so that additional
					resources are not consumed.*/
				
				/*the parent node of the current node*/
				node_t * dnp;

				/*obtain the parent node of the the current node*/
				err = ncache_node_lookup(np->nn->lnode->dir, &dnp);
				
				/*the lookup should never fail here*/
				assert(!err);
				
				/*open a port to the file we are interested in*/
				mach_port_t p = file_name_lookup_under
					(dnp->nn->port, np->nn->lnode->name, 0, 0);
	
				/*put `dnp` back, since we don't need it any more*/
				netfs_nput(dnp);

				if(!p)
					return EBADF;
				
				/*try to stat the node*/
				err = io_stat(p, &np->nn_stat);
				
				/*deallocate the port*/
				PORT_DEALLOC(p);
				}
			}
		}
	/*If we are at the root*/
	else
		/*put the size of the node into the stat structure belonging to `np`*/
		node_get_size(np, (OFFSET_T *)&np->nn_stat.st_size);

	/*Return the result of operations*/
	return err;
	}/*netfs_validate_stat*/
/*----------------------------------------------------------------------------*/
/*Syncs `node` completely to disk*/
error_t
netfs_attempt_sync
	(
	struct iouser * cred,
	struct node * node,
	int wait
	)
	{
	LOG_MSG("netfs_attempt_sync");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_sync*/
/*----------------------------------------------------------------------------*/
/*Fetches a directory*/
error_t
netfs_get_dirents
	(
	struct iouser * cred,
	struct node * dir,
	int first_entry,
	int num_entries,
	char ** data,
	mach_msg_type_number_t * data_len,
	vm_size_t max_data_len,
	int * data_entries
	)
	{
	LOG_MSG("netfs_get_dirents: '%s'", dir->nn->lnode->name);

	error_t err;

	/*Two pointers required for processing the list of dirents*/
	node_dirent_t * dirent_start, * dirent_current;
	
	/*The pointer to the beginning of the list of dirents*/
	node_dirent_t * dirent_list = NULL;
	
	/*The size of the current dirent*/
	size_t size = 0;
	
	/*The number of dirents added*/
	int count = 0;
	
	/*The dereferenced value of parameter `data`*/
	char * data_p;
	
	/*Takes into account the size of the given dirent*/
	int
	bump_size
		(
		const char * name
		)
		{
		/*If the required number of entries has not been listed yet*/
		if((num_entries == -1) || (count < num_entries))
			{
			/*take the current size and take into account the length of the name*/
			size_t new_size = size + DIRENT_LEN(strlen(name));
			
			/*If there is a limit for the received size and it has been exceeded*/
			if((max_data_len > 0) && (new_size > max_data_len))
				/*a new dirent cannot be added*/
				return 0;
				
			/*memorize the new size*/
			size = new_size;
			
			/*increase the number of dirents added*/
			++count;
			
			/*everything is OK*/
			return 1;
			}
		else
			{
			/*dirents cannot be added*/
			return 0;
			}
		}/*bump_size*/
		
	/*Adds a dirent to the list of dirents*/
	int
	add_dirent
		(
		const char * name,
		ino_t ino,
		int type
		)
		{
		/*If the required number of dirents has not been listed yet*/
		if((num_entries == -1) || (count < num_entries))
			{
			/*create a new dirent*/
			struct dirent hdr;

			/*obtain the length of the name*/
			size_t name_len = strlen(name);
			
			/*compute the full size of the dirent*/
			size_t sz = DIRENT_LEN(name_len);
			
			/*If there is no room for this dirent*/
			if(sz > size)
				/*stop*/
				return 0;
			else
				/*take into account the fact that a new dirent has just been added*/
				size -= sz;
				
			/*setup the dirent*/
			hdr.d_ino 	 = ino;
			hdr.d_reclen = sz;
			hdr.d_type   = type;
			hdr.d_namlen = name_len;
			
			/*The following two lines of code reflect the old layout of
				dirents in the memory. Now libnetfs expects the layout
				identical to the layout provided by dir_readdir (see dir_entries_get)*/

			/*copy the header of the dirent into the final block of dirents*/
			memcpy(data_p, &hdr, DIRENT_NAME_OFFS);
			
			/*copy the name of the dirent into the block of dirents*/
			strcpy(data_p + DIRENT_NAME_OFFS, name);
			
			/*This line is commented for the same reason as the two specifically
				commented lines above.*/
			/*move the current pointer in the block of dirents*/
			data_p += sz;
			
			/*count the new dirent*/
			++count;
			
			/*everything was OK, so say it*/
			return 1;
			}
		else
			/*no [new] dirents allowed*/
			return 0;
		}/*add_dirent*/
	
	/*List the dirents for node `dir`*/
	err = node_entries_get(dir, &dirent_list);
	
	/*If listing was successful*/
	if(!err)
		{
		/*find the entry whose number is `first_entry`*/
		for
			(
			dirent_start = dirent_list, count = 2;
			dirent_start && (count < first_entry);
			dirent_start = dirent_start->next, ++count
			);
			
		/*reset number of dirents added so far*/
		count = 0;
		
		/*make space for entries '.' and '..', if required*/
		if(first_entry == 0)
			bump_size(".");
		if(first_entry <= 1)
			bump_size("..");
			
		/*Go through all dirents*/
		for
			(
			dirent_current = dirent_start;
			dirent_current;
			dirent_current = dirent_current->next
			)
			/*If another dirent cannot be added succesfully*/
			if(bump_size(dirent_current->dirent->d_name) == 0)
				/*stop here*/
				break;
				
		/*allocate the required space for dirents*/
		*data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON, 0, 0);
		
		/*check if any error occurred*/
		err = ((void *)*data == MAP_FAILED) ? (errno) : (0);
		}
		
	/*If no errors have occurred so far*/
	if(!err)
		{
		/*obtain the pointer to the beginning of the block of dirents*/
		data_p = *data;

		/*fill the parameters with useful values*/
		*data_len = size;
		*data_entries = count;
		
		/*reset the number of dirents added*/
		count = 0;

		/*add entries '.' and '..', if required*/
		if(first_entry == 0)
			add_dirent(".", 2, DT_DIR);
		if(first_entry <= 1)
			add_dirent("..", 2, DT_DIR);

		/*Follow the list of dirents beginning with dirents_start*/
		for
			(
			dirent_current = dirent_start; dirent_current;
			dirent_current = dirent_current->next
			)
			/*If the addition of the current dirent fails*/
			if
				(
				add_dirent
					(dirent_current->dirent->d_name, dirent_current->dirent->d_fileno,
					dirent_current->dirent->d_type) == 0
				)
				/*stop adding dirents*/
				break;
		}
		
	/*If the list of dirents has been allocated, free it*/
	if(dirent_list)
		node_entries_free(dirent_list);
		
	/*The directory has been read right now, modify the access time*/
	fshelp_touch(&dir->nn_stat, TOUCH_ATIME, maptime);
	
	/*Return the result of listing the dirents*/
	return err;
	}/*netfs_get_dirents*/
/*----------------------------------------------------------------------------*/
/*Looks up `name` under `dir` for `user`*/
error_t
netfs_attempt_lookup
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	struct node ** node
	)
	{
	LOG_MSG("netfs_attempt_lookup: '%s'", name);

	error_t err = 0;

	/*If we are asked to fetch the current directory*/
	if(strcmp(name, ".") == 0)
		{
		/*add a reference to `dir` and put it into `node`*/
		netfs_nref(dir);
		*node = dir;
		
		/*everything is OK*/
		return 0;
		}
	/*If we are asked to fetch the parent directory*/
	else if(strcmp(name, "..") == 0)
		{
		/*If the supplied node is not root*/
		if(dir->nn->lnode->dir)
			{
			/*The node corresponding to the parent directory must exist here*/
			assert(dir->nn->lnode->dir->node);
			
			/*put the parent node of `dir` into the result*/
			err = ncache_node_lookup(dir->nn->lnode->dir, node);
			}
		/*The supplied node is root*/
		else
			{
			/*this node is not included into our filesystem*/
			err = ENOENT;
			*node = NULL;
			}
			
		/*unlock the directory*/
		mutex_unlock(&dir->lock);
		
		/*stop here*/
		return err;
		}

	/*Checks whether the given name satisfied the required property*/
	int
	check_property
		(
		const char * name
		)
		{
		/*If there is no property*/
		if(!property)
			/*no filtering will be applied, any name is OK*/
			return 0;
		
		/*The number of occurrences of PROPERTY_PARAM in the property*/
		int param_entries_count = 0;

		/*The pointer to the current occurrence of PROPERTY_PARAM*/
		char * occurrence = strstr(property, PROPERTY_PARAM);
		
		/*Count the number of occurrences*/
		for(; occurrence;
			occurrence = strstr(occurrence + 1, PROPERTY_PARAM),
			++param_entries_count);
		
		/*Compute the length of the property param*/
		size_t property_param_len = strlen(PROPERTY_PARAM);
		
		/*The length of the property*/
		size_t property_len = (property) ? (strlen(property)) : (0);
	
		/*Everything OK at first*/
		err = 0;
		
		/*Compute the length of the full name once*/
		size_t full_name_len = strlen(dir->nn->lnode->path) + 1 + strlen(name) + 1;
		
		/*Try to allocate the required space*/
		char * full_name = malloc(full_name_len);
		if(!full_name)
			{
			err = ENOMEM;
			return 0;
			}
		
		/*Initialize `full_name` as a valid string*/
		full_name[0] = 0;
		
		/*Construct the full name*/
		strcpy(full_name, dir->nn->lnode->path);
		strcat(full_name, "/");
		strcat(full_name, name);
		
		LOG_MSG("netfs_attempt_lookup: Applying filter to %s...", full_name);
		
		/*Compute the space required for the final filtering command*/
		size_t sz = property_len + (strlen(full_name) - property_param_len)
			* param_entries_count;
		
		/*Try to allocate the space for the command*/
		char * cmd = malloc(sz);
		if(!cmd)
			{
			free(full_name);
			err = ENOMEM;
			return 0;
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

	/*If the given name does not satisfy the property*/
	if(check_property(name) != 0)
		{
		/*unlock the directory*/
		mutex_unlock(&dir->lock);

		/*no such file in the directory*/
		return ENOENT;
		}

	/*Try to lookup the given file in the underlying directory*/
	mach_port_t p = file_name_lookup_under(dir->nn->port, name, 0, 0);
	
	/*If the lookup failed*/
	if(p == MACH_PORT_NULL)
		{
		/*unlock the directory*/
		mutex_unlock(&dir->lock);

		/*no such entry*/
		return ENOENT;
		}

	/*Obtain the stat information about the file*/
	io_statbuf_t stat;
	err = io_stat(p, &stat);
	
	/*Deallocate the obtained port*/
	PORT_DEALLOC(p);

	/*If this file is not a directory*/
	if(err || !S_ISDIR(stat.st_mode))
		{
		/*do not set the port*/
		p = MACH_PORT_NULL;
		}
	else
		{
		/*lookup the port with the right to read the contents of the directory*/
		p = file_name_lookup_under(dir->nn->port, name, O_READ | O_DIRECTORY, 0);
		if(p == MACH_PORT_NULL)
			{
			return EBADF; /*not enough rights?*/
			}
		}

	/*The lnode corresponding to the entry we are supposed to fetch*/
	lnode_t * lnode;
	
	/*Finalizes the execution of this function*/
	void
	finalize(void)
		{
		/*If some errors have occurred*/
		if(err)
			{
			/*the user should receive nothing*/
			*node = NULL;
			
			/*If there is some port, free it*/
			if(p != MACH_PORT_NULL)
				PORT_DEALLOC(p);
			}
		/*If there is a node to return*/
		if(*node)
			{
			/*unlock the node*/
			mutex_unlock(&(*node)->lock);
			
			/*add the node to the cache*/
			ncache_node_add(*node);
			}
		
		/*Unlock the mutexes in `dir`*/
		mutex_unlock(&dir->nn->lnode->lock);
		mutex_unlock(&dir->lock);
		}/*finalize*/

	/*Try to find an lnode called `name` under the lnode corresponding to `dir`*/
	err = lnode_get(dir->nn->lnode, name, &lnode);
	
	/*If such an entry does not exist*/
	if(err == ENOENT)
		{
		/*create a new lnode with the supplied name*/
		err = lnode_create(name, &lnode);
		if(err)
			{
			finalize();
			return err;
			}
		
		/*install the new lnode into the directory*/
		lnode_install(dir->nn->lnode, lnode);
		}
	
	/*Obtain the node corresponding to this lnode*/
	err = ncache_node_lookup(lnode, node);
	
	/*Remove an extra reference from the lnode*/
	lnode_ref_remove(lnode);
	
	/*If the lookup in the cache failed*/
	if(err)
		{
		/*stop*/
		finalize();
		return err;
		}

	/*Store the port in the node*/
	(*node)->nn->port = p;

	/*Construct the full path to the node*/
	err = lnode_path_construct(lnode, NULL);
	if(err)
		{
		finalize();
		return err;
		}
	
	/*Now the node is up-to-date*/
	(*node)->nn->flags = FLAG_NODE_ULFS_UPTODATE;

	/*Return the result of performing the operations*/
	finalize();
	return err;
	}/*netfs_attempt_lookup*/
/*----------------------------------------------------------------------------*/
/*Deletes `name` in `dir` for `user`*/
error_t
netfs_attempt_unlink
	(
	struct iouser * user,
	struct node * dir,
	char * name
	)
	{
	LOG_MSG("netfs_attempt_unlink");

	return 0;
	}/*netfs_attempt_unlink*/
/*----------------------------------------------------------------------------*/
/*Attempts to rename `fromdir`/`fromname` to `todir`/`toname`*/
error_t
netfs_attempt_rename
	(
	struct iouser * user,
	struct node * fromdir,
	char * fromname,
	struct node * todir,
	char * toname,
	int excl
	)
	{
	LOG_MSG("netfs_attempt_rename");

	/*Operation not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_rename*/
/*----------------------------------------------------------------------------*/
/*Attempts to create a new directory*/
error_t
netfs_attempt_mkdir
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	mode_t mode
	)
	{
	LOG_MSG("netfs_attempt_mkdir");

	return 0;
	}/*netfs_attempt_mkdir*/
/*----------------------------------------------------------------------------*/
/*Attempts to remove directory `name` in `dir` for `user`*/
error_t
netfs_attempt_rmdir
	(
	struct iouser * user,
	struct node * dir,
	char * name
	)
	{
	LOG_MSG("netfs_attempt_rmdir");

	return 0;
	}/*netfs_attempt_rmdir*/
/*----------------------------------------------------------------------------*/
/*Attempts to change the mode of `node` for user `cred` to `uid`:`gid`*/
error_t
netfs_attempt_chown
	(
	struct iouser * cred,
	struct node * node,
	uid_t uid,
	uid_t gid
	)
	{
	LOG_MSG("netfs_attempt_chown");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_chown*/
/*----------------------------------------------------------------------------*/
/*Attempts to change the author of `node` to `author`*/
error_t
netfs_attempt_chauthor
	(
	struct iouser * cred,
	struct node * node,
	uid_t author
	)
	{
	LOG_MSG("netfs_attempt_chauthor");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_chauthor*/
/*----------------------------------------------------------------------------*/
/*Attempts to change the mode of `node` to `mode` for `cred`*/
error_t
netfs_attempt_chmod
	(
	struct iouser * user,
	struct node * node,
	mode_t mode
	)
	{
	LOG_MSG("netfs_attempt_chmod");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_chmod*/
/*----------------------------------------------------------------------------*/
/*Attempts to turn `node` into a symlink targetting `name`*/
error_t
netfs_attempt_mksymlink
	(
	struct iouser * cred,
	struct node * node,
	char * name
	)
	{
	LOG_MSG("netfs_attempt_mksymlink");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_mksymlink*/
/*----------------------------------------------------------------------------*/
/*Attempts to turn `node` into a device; type can be either S_IFBLK or S_IFCHR*/
error_t
netfs_attempt_mkdev
	(
	struct iouser * cred,
	struct node * node,
	mode_t type,
	dev_t indexes
	)
	{
	LOG_MSG("netfs_attempt_mkdev");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_mkdev*/
/*----------------------------------------------------------------------------*/
/*Attempts to set the passive translator record for `file` passing `argz`*/
error_t
netfs_set_translator
	(
	struct iouser * cred,
	struct node * node,
	char * argz,
	size_t arglen
	)
	{
	LOG_MSG("netfs_set_translator");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_set_translator	*/
/*----------------------------------------------------------------------------*/
/*Attempts to call chflags for `node`*/
error_t
netfs_attempt_chflags
	(
	struct iouser * cred,
	struct node * node,
	int flags
	)
	{
	LOG_MSG("netfs_attempt_chflags");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_chflags*/
/*----------------------------------------------------------------------------*/
/*Attempts to set the size of file `node`*/
error_t
netfs_attempt_set_size
	(
	struct iouser * cred,
	struct node * node,
	loff_t size
	)
	{
	LOG_MSG("netfs_attempt_set_size");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_set_size*/
/*----------------------------------------------------------------------------*/
/*Fetches the filesystem status information*/
error_t
netfs_attempt_statfs
	(
	struct iouser * cred,
	struct node * node,
	fsys_statfsbuf_t * st
	)
	{
	LOG_MSG("netfs_attempt_statfs");

	/*Operation is not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_statfs*/
/*----------------------------------------------------------------------------*/
/*Syncs the filesystem*/
error_t
netfs_attempt_syncfs
	(
	struct iouser * cred,
	int wait
	)
	{
	LOG_MSG("netfs_attempt_syncfs");

	/*Everythin OK*/
	return 0;
	}/*netfs_attempt_syncfs*/
/*----------------------------------------------------------------------------*/
/*Creates a link in `dir` with `name` to `file`*/
error_t
netfs_attempt_link
	(
	struct iouser * user,
	struct node * dir,
	struct node * file,
	char * name,
	int excl
	)
	{
	LOG_MSG("netfs_attempt_link");

	/*Operation not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_link*/
/*----------------------------------------------------------------------------*/
/*Attempts to create an anonymous file related to `dir` with `mode`*/
error_t
netfs_attempt_mkfile
	(
	struct iouser * user,
	struct node * dir,
	mode_t mode,
	struct node ** node
	)
	{
	LOG_MSG("netfs_attempt_mkfile");

	/*Unlock the directory*/
	mutex_unlock(&dir->lock);

	/*Operation not supported*/
	return EOPNOTSUPP;
	}/*netfs_attempt_mkfile*/
/*----------------------------------------------------------------------------*/
/*Reads the contents of symlink `node` into `buf`*/
error_t
netfs_attempt_readlink
	(
	struct iouser * user,
	struct node * node,
	char * buf
	)
	{
	LOG_MSG("netfs_attempt_readlink");

	/*Operation not supported (why?..)*/
	return EOPNOTSUPP;
	}/*netfs_attempt_readlink*/
/*----------------------------------------------------------------------------*/
/*Reads from file `node` up to `len` bytes from `offset` into `data`*/
error_t
netfs_attempt_read
	(
	struct iouser * cred,
	struct node * np,
	loff_t offset,
	size_t * len,
	void * data
	)
	{
	LOG_MSG("netfs_attempt_read");

	error_t err = 0;

	/*If there is no port open for the current node*/
	if(np->nn->port == MACH_PORT_NULL)
		{
		/*the parent node of the current node*/
		node_t * dnp;

		/*obtain the parent node of the the current node*/
		err = ncache_node_lookup(np->nn->lnode->dir, &dnp);
		
		/*the lookup should never fail here*/
		assert(!err);
		
		/*open a port to the file we are interested in*/
		mach_port_t p = file_name_lookup_under
			(dnp->nn->port, np->nn->lnode->name, O_READ, 0);

		/*put `dnp` back, since we don't need it any more*/
		netfs_nput(dnp);

		if(!p)
			return EBADF;
			
		/*store the port in the node*/
		np->nn->port = p;
		}
		
	/*Read the required data from the file*/
	err = io_read(np->nn->port, (char **)&data, len, offset, *len);

	/*Return the result of reading*/
	return err;
	}/*netfs_attempt_read*/
/*----------------------------------------------------------------------------*/
/*Writes to file `node` up to `len` bytes from offset from `data`*/
error_t
netfs_attempt_write
	(
	struct iouser * cred,
	struct node * node,
	loff_t offset,
	size_t * len,
	void * data
	)
	{
	LOG_MSG("netfs_attempt_write");

	return 0;
	}/*netfs_attempt_write*/
/*----------------------------------------------------------------------------*/
/*Frees all storage associated with the node*/
void
netfs_node_norefs
	(
	struct node * np
	)
	{
	/*Destroy the node*/
	node_destroy(np);
	}/*netfs_node_norefs*/
/*----------------------------------------------------------------------------*/
/*Entry point*/
int
main
	(
	int argc,
	char ** argv
	)
	{
	/*Start logging*/
	INIT_LOG();
	LOG_MSG(">> Starting initialization...");

	/*The port on which this translator will be set upon*/
	mach_port_t bootstrap_port;
	
	error_t err = 0;
	
	/*Parse the command line arguments*/
	argp_parse(&argp_startup, argc, argv, ARGP_IN_ORDER, 0, 0);
	LOG_MSG("Command line arguments parsed.");
	
	/*Try to create the root node*/
	err = node_create_root(&netfs_root_node);
	if(err)
		error(EXIT_FAILURE, err, "Failed to create the root node");
	LOG_MSG("Root node created.");

	/*Obtain the bootstrap port*/
	task_get_bootstrap_port(mach_task_self(), &bootstrap_port);

	/*Initialize the translator*/
	netfs_init();

	/*Obtain a port to the underlying node*/
	underlying_node = netfs_startup(bootstrap_port, O_READ);
	LOG_MSG("netfs initialization complete.");

	/*Initialize the root node*/
	err = node_init_root(netfs_root_node);
	if(err)
		error(EXIT_FAILURE, err, "Failed to initialize the root node");
	LOG_MSG("Root node initialized.");
	LOG_MSG("\tRoot node address: 0x%lX", (unsigned long)netfs_root_node);
	
	/*Map the time for updating node information*/
	err = maptime_map(0, 0, &maptime);
	if(err)
		error(EXIT_FAILURE, err, "Failed to map the time");
	LOG_MSG("Time mapped.");
		
	/*Initialize the cache with the required number of nodes*/
	ncache_init(ncache_size);
	LOG_MSG("Cache initialized.");
	
	/*Obtain stat information about the underlying node*/
	err = io_stat(underlying_node, &underlying_node_stat);
	if(err)
		error(EXIT_FAILURE, err,
			"Cannot obtain stat information about the underlying node");
	LOG_MSG("Stat information for undelying node obtained.");
		
	/*Obtain the ID of the current process*/
	fsid = getpid();

	/*Setup the stat information for the root node*/
	netfs_root_node->nn_stat = underlying_node_stat;

	netfs_root_node->nn_stat.st_ino 	= FILTERFS_ROOT_INODE;
	netfs_root_node->nn_stat.st_fsid	= fsid;
	netfs_root_node->nn_stat.st_mode = S_IFDIR | (underlying_node_stat.st_mode
		& ~S_IFMT & ~S_ITRANS); /*we are providing a translated directory*/
		
	netfs_root_node->nn_translated = netfs_root_node->nn_stat.st_mode;
	
	/*If the underlying node is not a directory, enhance the permissions
		of the root node of filterfs*/
	if(!S_ISDIR(underlying_node_stat.st_mode))
		{
		/*can be read by owner*/
		if(underlying_node_stat.st_mode & S_IRUSR)
			/*allow execution by the owner*/
			netfs_root_node->nn_stat.st_mode |= S_IXUSR;
		/*can be read by group*/
		if(underlying_node_stat.st_mode & S_IRGRP)
			/*allow execution by the group*/
			netfs_root_node->nn_stat.st_mode |= S_IXGRP;
		/*can be read by others*/
		if(underlying_node_stat.st_mode & S_IROTH)
			/*allow execution by the others*/
			netfs_root_node->nn_stat.st_mode |= S_IXOTH;
		}
		
	/*Update the timestamps of the root node*/
	fshelp_touch
		(&netfs_root_node->nn_stat, TOUCH_ATIME | TOUCH_MTIME | TOUCH_CTIME,
		maptime);

	LOG_MSG(">> Initialization complete. Entering netfs server loop...");
	
	/*Start serving clients*/
	for(;;)
		netfs_server_loop();
	}/*main*/
/*----------------------------------------------------------------------------*/
