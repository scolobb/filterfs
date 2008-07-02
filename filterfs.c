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
#include "node.h"
#include "options.h"
#include "ncache.h"
#include "lib.h"
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
	/*Unlock `dir` and say that we can do nothing else here*/
	mutex_unlock(&dir->lock);
	return EOPNOTSUPP;
	}/*netfs_attempt_create_file*/
/*----------------------------------------------------------------------------*/
/*A local function for attempting to create a file; we create this because we
	use a custom netfs_S_dir_lookup*/
error_t
netfs_attempt_create_file_reduced
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	mode_t mode,
	int flags
	)
	{
	error_t err;

	/*The port to the file which will be created*/
	mach_port_t p;
	
	/*Stat information about the file which will be created*/
	io_statbuf_t statbuf;
	
	/*Force update of the directory*/
	node_update(dir);
	
	/*See whether the user has the right to modify `dir`*/
	err = fshelp_checkdirmod(&dir->nn_stat, NULL, user);
	if(err)
		{
		mutex_unlock(&dir->lock);
		return err;
		}
	
	/*If this is a special case of a no UID process (like login shell)*/
	if(!user->uids->ids || !user->gids->ids)
		{
		/*fail the operation*/
		err = EACCES;
		mutex_unlock(&dir->lock);
		return err;
		}
	
	/*Attempt to create the file (the directory has to be unlocked)*/
	mutex_unlock(&dir->lock);
	err = node_lookup_file(dir, name, flags | O_CREAT, &p, &statbuf);
	
	/*If the lookup failed, stop*/
	if(err)
		return err;
		
	/*Lock the directory again*/
	mutex_lock(&dir->lock);
	
	/*Try change the the mode of the file to the one requested in the parameters*/
	err = file_chmod(p, mode);
	if(err)
		{
		/*close the port and unlink the newly created file*/
		PORT_DEALLOC(p);
		node_unlink_file(dir, name);
		
		/*stop*/
		mutex_unlock(&dir->lock);
		return err;
		}
	
	/*Change the ownership of the file to the one specified in the parameters*/
	err = file_chown(p, user->uids->ids[0], user->gids->ids[0]);
	if(err)
		{
		/*close the port and unlink the newly created file*/
		PORT_DEALLOC(p);
		node_unlink_file(dir, name);
		
		/*stop*/
		mutex_unlock(&dir->lock);
		return err;
		}
	
	/*Obtain the stat information about the file*/
	err = io_stat(p, &statbuf);
	
	/*Check file permissions*/
	if(!err && (flags & O_READ))
		err = fshelp_access(&statbuf, S_IREAD, user);
	if(!err && (flags & O_WRITE))
		err = fshelp_access(&statbuf, S_IWRITE, user);
	if(!err && (flags & O_EXEC))
		err = fshelp_access(&statbuf, S_IEXEC, user);
	
	/*If an error happened in the previous four operations*/
	if(err)
		{
		/*close the port and unlink the newly created file*/
		PORT_DEALLOC(p);
		node_unlink_file(dir, name);
		
		/*stop*/
		mutex_unlock(&dir->lock);
		return err;
		}
		
	/*Close the port to the newly created file*/
	PORT_DEALLOC(p);
	
	/*Unlock the directory*/
	mutex_unlock(&dir->lock);
	return err;
	}/*netfs_attempt_create_file_reduced*/
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
	error_t err;
	
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
	return 0;
	}/*netfs_attempt_utimes*/
/*----------------------------------------------------------------------------*/
/*Returns the valid access types for file `node` and user `cred`*/
error_t
netfs_report_access
	(
	struct iouser * cred,
	struct node * node,
	int * types
	)
	{
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
	error_t err;
	
	/*If we are not at the root*/
	if(np != netfs_root_node)
		{
		/*If the node is not surely up-to-date*/
		if(!(np->nn->flags & FLAG_NODE_ULFS_UPTODATE))
			/*update it*/
			err = node_update(np);
		
		/*If no errors have yet occurred*/
		if(!err)
			{
			/*If the port to the file corresponding to `np` is valid*/
			if(np->nn->port != MACH_PORT_NULL)
				{
				/*attempt to stat this file*/
				err = io_stat(np->nn->port, &np->nn_stat);

				/*If stat information has been successfully obtained for the file*/
				if(!err)
					/*duplicate the st_mode field of stat structure*/
					np->nn_translated = np->nn_stat.st_mode;
				}
			else
				err = 1;
			
			/*If some error occurred during previous operations*/
			if(err)
				/*say that an error has occurred*/
				err = ENOENT;	/*FIXME: It this the right error code in this case?*/
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
			
			/*copy the header of the dirent into the final block of dirents*/
			memcpy(data_p, &hdr, DIRENT_NAME_OFFS);
			
			/*copy the name of the dirent into the block of dirents*/
			strcpy(data_p + DIRENT_NAME_OFFS, name);
			
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
/*Looks up `name` under `dir` for `user` (not used here)*/
error_t
netfs_attempt_lookup
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	struct node ** node
	)
	{
	/*Do not use this function*/
	return EOPNOTSUPP;
	}/*netfs_attempt_lookup*/
/*----------------------------------------------------------------------------*/
/*Performs an improved look up of `name` under `dir` for `user`*/
error_t
netfs_attempt_lookup_improved
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	struct node ** np,
	int flags,
	int lastcomp,					/*1 if we are at the last component of the path(?)*/
	mach_port_t * port,
	mach_msg_type_name_t * port_type
	)
	{
	error_t err;

	/*Finalizes the execution of this function*/
	void
	finalize(void)
		{
		/*If there are some errors*/
		if(err)
			/*reset the pointer to the node*/
			*np = NULL;
		/*If the pointer to the looked up node exists*/
		else if(*np)
			{
			/*give away the exclusive access to the node*/
			mutex_unlock(&(*np)->lock);
			
			/*add the looked up node to the cache*/
			ncache_node_add(*np);
			}
		
		/*unlock the mutexes in `dir`*/
		mutex_unlock(&dir->nn->lnode->lock);
		mutex_unlock(&dir->lock);
		}/*finalize*/
	
	/*The port to the file, which is to be looked up*/
	mach_port_t p;
	
	/*Gain a lock on the give node*/
	mutex_lock(&dir->nn->lnode->lock);
	
	/*See if the user has execute right on dir*/
	err = fshelp_access(&dir->nn_stat, S_IEXEC, user);
	if(err)
		{
		finalize();
		return err;
		}
	
	/*If no name was specified or if the user requests the current directory*/
	if(!name || (strcmp(name, ".") == 0))
		{
		/*return `dir` as the result*/
		*np = dir;
		
		/*add a reference to *`np` (`dir`, actually)*/
		netfs_nref(*np);
		}
	/*If the user requests the parent directory*/
	else if(strcmp(name, "..") == 0)
		{
		/*obtain the lnode corresponding to `dir`*/
		lnode_t * lnode = dir->nn->lnode;

		/*the parent node of node `dir`*/
		node_t * node;
		
		/*obtain the parent node of `dir`*/
		err = ncache_node_lookup(lnode->dir, &node);
		if(err)
			{
			finalize();
			return err;
			}
		
		/*store the pointer to the parent node in `np`*/
		*np = node;
		}
	/*If the user requests a general file*/
	else
		{
		/*obtain the lnode for `dir`*/
		lnode_t * dir_lnode = dir->nn->lnode;
		
		/*stat information about the looked up file*/
		io_statbuf_t statbuf;
		
		/*the lnode corresponding to the file we will be looking up*/
		lnode_t * lnode = NULL;
		
		/*try update `dir`*/
		err = node_update(dir);
		if(err)
			{
			finalize();
			return err;
			}
		
		/*we have to unlock `dir` while doing lookups*/
		mutex_unlock(&dir_lnode->lock);
		mutex_unlock(&dir->lock);
		
		/*lookup the file*/
		/*fail if the file does not exist and do not create it in unlinked state*/
		err = node_lookup_file
			(dir, name, flags & ~(O_CREAT | O_NOLINK), &p, &statbuf);
			
		/*lock the mutexes back*/
		mutex_lock(&dir->lock);
		mutex_lock(&dir_lnode->lock);
		
		/*if the lookup failed, stop*/
		if(err)
			{
			finalize();
			return err;
			}
			
		/*If the obtained node is a directory*/
		if(S_ISDIR(statbuf.st_mode))
			{
			/*the node corresponding to file we are interested in*/
			node_t * node;
			
			/*drop the port, since we don't need it directly*/
			PORT_DEALLOC(p);
			
			/*try to get the lnode with name `name`*/
			err = lnode_get(dir_lnode, name, &lnode);
			
			/*If such a node does not exist*/
			if(err == ENOENT)
				{
				/*try to create a new lnode with the given name*/
				err = lnode_create(name, &lnode);
				if(err)
					{
					finalize();
					return err;
					}
					
				/*install the new lnode in `dir`*/
				lnode_install(dir_lnode, lnode);
				}
			
			/*obtain the node corresponding to `lnode`*/
			err = ncache_node_lookup(lnode, &node);
			
			/*unlock `lnode` for this thread*/
			lnode_ref_remove(lnode);
			
			/*If the node corresponding to `lnode` could not be found*/
			if(err)
				{
				/*stop*/
				finalize();
				return err;
				}
			
			/*store the resulting node in the corresponding parameter*/
			*np = node;
			}
		/*The obtained node is not a directory*/
		else
			{
			/*the port to the requested file with the access restricted by
				`user` and the rights of this process*/
			mach_port_t p_restricted;
			
			/*If we are not at the last component*/
			if(!lastcomp)
				{
				/*release the port*/
				PORT_DEALLOC(p);
				
				/*return information that the requested file is not a directory*/
				err = ENOTDIR;
				finalize();
				return err;
				}
				
			/*check whether the user has the rights to access the file the way
				they requested*/
			if(flags & O_READ)
				err = fshelp_access(&statbuf, S_IREAD, user);
			if(!err && (flags & O_WRITE))
				err = fshelp_access(&statbuf, S_IWRITE, user);
			if(!err && (flags & O_EXEC))
				err = fshelp_access(&statbuf, S_IEXEC, user);
			
			/*if the user does not have the requiered priveleges, stop*/
			if(err)
				{
				PORT_DEALLOC(p);
				finalize();
				return err;
				}
			
			/*obtain a new port restricted to the rights of `user`*/
			err = io_restrict_auth
				(
				p, &p_restricted,
				user->uids->ids, user->uids->num,
				user->gids->ids, user->gids->num
				);
			
			/*release the intial port*/
			PORT_DEALLOC(p);
			
			/*if restricting rights failed, stop*/
			if(err)
				{
				finalize();
				return err;
				}
				
			/*copy the result into the parameter*/
			*port = p_restricted;
			*port_type = MACH_MSG_TYPE_MOVE_SEND;
			}
		}
	
	/*Perform finalization*/
	finalize();
	return err;
	}/*netfs_attempt_lookup_improved*/
/*----------------------------------------------------------------------------*/
/*A special implementation of netfs_S_dir_lookup, because libnetfs does not
	know about cases in which the server wants to return (foreign) ports exactly
	to the user, instead of usual node structures.
	This comment was taken from the code for unionfs, and the things stated here
	might not be valid now (2008/06/30).*/
error_t
netfs_S_dir_lookup
	(
	struct protid * diruser,
	char * filename,
	int flags,
	mode_t mode,
	retry_type * do_retry,
	char * retry_name,
	mach_port_t * retry_port,
	mach_msg_type_name_t * retry_port_type
	)
	{
	struct node * dnp, * np;
	error_t err = 0;

	/*True if O_CREAT is set*/
	int create = (flags & O_CREAT);
	
	/*True it O_EXCL is set*/
	int excl = (flags & O_EXCL);
	
	/*True if the result must be S_IFDIR*/
	int mustbedir = 0;
	
	/*True if we are at the last component of the path*/
	int lastcomp = 0;
	
	/*True if this node is newly created*/
	int newnode = 0;
	
	/*The number of symlinks already followed in tracing the route to the file*/
	int nsymlinks;
	
	/*The maximal number of lookup retries*/
	int maxretries = 10;

	/*The name of the next component in the path*/
	char * nextname;
	
	/*A helper protid and user*/
	struct protid * newpi;
	struct iouser * user;
	
	/*Performs finalization*/
	void
	finalize(void)
		{
		/*Put back the node pointed by `np`, if it exists*/
		if(np)
			netfs_nput(np);
			
		/*Put back the node pointed by `dnp`, it it exists*/
		if(dnp)
			netfs_nput(dnp);
		}/*finalize*/
		
	/*Prepares to return `np` as the result*/
	void
	return_np(void)
		{
		/*If a directory is required, and there is no node*/
		if(mustbedir && !np)
			{
			/*set the corresponding error code and exit*/
			err = ENOTDIR;
			finalize();
			return;
			}
			
		/*If the node has been created*/
		if(np)
			{
			/*check whether the opening is allowed to complete*/
			err = netfs_check_open_permissions(diruser->user, np, flags, newnode);
			
			/*If an error has occurred*/
			if(err)
				{
				/*perform finalization and stop*/
				finalize();
				return;
				}

			/*remove open-only flags*/
			flags &= ~OPENONLY_STATE_MODES;
		
			/*try to duplicate the user referenced by `diruser`*/
			err = iohelp_dup_iouser(&user, diruser->user);
			if(err)
				{
				finalize();
				return;
				}
			
			/*create a new peropen for the new user and obtain their credentials*/
			newpi = netfs_make_protid
				(netfs_make_peropen(np, flags, diruser->po), user);
			
			/*If the creation of credentials failed*/
			if(newpi)
				{
				/*dispose of `user`*/
				iohelp_free_iouser(user);
				
				/*setup the error code*/
				err = errno;
				
				/*perform finalization and stop*/
				finalize();
				return;
				}
			
			/*get the name of the receive right associated with `newpi`*/
			*retry_port = ports_get_right(newpi);
			
			/*drop the reference to `newpi`*/
			ports_port_deref(newpi);
			}
		
		/*Perform finalization*/
		finalize();
		}/*return_np*/
		
	/*Perform the lookup (possibly, retry)*/
	void
	do_lookup(void)
		{
		/*If the number of retries has been exceeded, die, because normally
			only one retry is required*/
		assert(maxretries);
		
		/*Count this try*/
		--maxretries;

		/*If the directory is the root node of the filterfs filesystem and
			we are requested to fetch '..'*/
		if(
			((dnp == netfs_root_node) || (dnp == diruser->po->shadow_root))
			&& ((filename[0] == '.') && (filename[1] == '.') && (filename[2] == 0))
			)
			{
			/*If we are at the root of the shadow tree*/
			if(dnp == diruser->po->shadow_root)
				{
				/*the lookup should be retried after reauthenticating the retry port*/
				*do_retry = FS_RETRY_REAUTH;
				
				/*the retrial should be done on the parent of the root node of the
					shadow tree*/
				*retry_port = diruser->po->shadow_root_parent;
				
				/*set the type of the retry port*/
				*retry_port_type = MACH_MSG_TYPE_COPY_SEND;
				
				/*If we are not at the last component*/
				if(!lastcomp)
					/*copy the remaining part of the file name into `retry_name`*/
					strcpy(retry_name, nextname);
					
				/*no error*/
				err = 0;
				
				/*unlock the directory*/
				mutex_unlock(&dnp->lock);
				
				/*finalize and exit*/
				finalize();
				return;
				}
			/*If the port to the parent node of the root node of the translator
				is not null*/
			else if(diruser->po->root_parent != MACH_PORT_NULL)
				/*We are at the real translator root. Even if diuser->po has a shadow
					root, we can get here if it is in a directory that was renamed out
					from under it*/
				{
				/*retry after reauthenticating the retry port*/
				*do_retry = FS_RETRY_REAUTH;
				
				/*do the retrial on the parent of the real translator root*/
				*retry_port = diruser->po->root_parent;
				
				/*set the type of retry port*/
				*retry_port_type = MACH_MSG_TYPE_COPY_SEND;
				
				/*If we are not at the last component of the path*/
				if(!lastcomp)
					/*copy the remaining part of the file name into `retry_name`*/
					strcpy(retry_name, nextname);
				
				/*no error*/
				err = 0;
				
				/*unlock the directory*/
				mutex_unlock(&dnp->lock);
				
				/*perform finalization and exit*/
				finalize();
				return;
				}
			/*If we are at the global root*/
			else
				{
				/*no error*/
				err = 0;
				
				/*make `np` and `dnp` point to the same node*/
				np = dnp;
				
				/*add a reference to `np`*/
				netfs_nref(np);
				}
			}
		/*We are not at the root or we are not required to fetch `..`*/
		else
			/*attempt a lookup on the current file name component*/
			err = netfs_attempt_lookup_improved
				(diruser->user, dnp, filename, &np, flags, lastcomp,
				retry_port, retry_port_type);
				
		/*If O_EXCL has been specified, and the specified file already exists*/
		if(lastcomp && create && excl && !err && np)
			/*say that such a file already exists*/
			err = EEXIST;
			
		/*If it is necessary to create a new node*/
		if(lastcomp && create && (err == ENOENT))
			{
			/*safely remove some unnecessary bits from mode*/
			mode &= ~(S_IFMT | S_ISPARE | S_ISVTX);
			
			/*force the creation of a regular file*/
			mode |= S_IFREG;
			
			/*lock the directory*/
			mutex_lock(&dnp->lock);
			
			/*attempt to create the required file*/
			err = netfs_attempt_create_file_reduced
				(diruser->user, dnp, filename, mode, flags);
				
			/*We retry lookup in two cases:
				- we created the file and we want to get a valid port;
				- someone has already created the file (between out lookup
					and this create) then we just got EEXIST. If we are asked
					for O_EXCL that's fine; otherwise, we have to retry the lookup*/
			if(!err || ((err == EEXIST) && !excl))
				{
				/*lock the directory again*/
				mutex_lock(&dnp->lock);
				
				/*retry the lookup*/
				do_lookup();	/*the recursion shouldn't normally go deeper than 2
												levels*/
				}

			/*remark that a new node has been created*/
			newnode = 1;
			}
		}/*do_lookup*/

	/*If no user has been specified, stop with an error*/
	if(!diruser)
		return EOPNOTSUPP;
	
	/*Skip the leading slashes in the filename*/
	for(; *filename == '/'; ++filename);
	
	/*Set the type of retry port*/
	*retry_port_type = MACH_MSG_TYPE_MAKE_SEND;
	
	/*Perform a normal retry*/
	*do_retry = FS_RETRY_NORMAL;
	*retry_name = 0;
	
	/*If the filename is void*/
	if(*filename == 0)
		{
		/*return the same file as was given*/
		np = diruser->po->np;
		dnp = NULL;
		
		/*lock the node and a reference to it*/
		mutex_lock(&np->lock);
		netfs_nref(np);
		
		/*stop*/
		return_np();
		return err;
		}
	
	/*Obtain a pointer to the node corresponding to `diruser` credentials*/
	dnp = diruser->po->np;
	
	/*Lock the pointer to the directory*/
	mutex_lock(&dnp->lock);
	
	/*Acquire a reference to `dnp`, so that we will be able to netfs_nput later*/
	netfs_nref(dnp);
	
	/*While the entire filename has not been processed*/
	do
		{
		/*die if we are at the last component of the path*/
		assert(lastcomp != 0);
		
		/*find the next component of the path*/
		nextname = index(filename, '/');
		
		/*If there are slashes in the current filename*/
		if(nextname)
			{
			/*substitute the current slash in the path with 0 and go forward*/
			*nextname++ = 0;
			
			/*skip the leading slashes*/
			for(; *nextname == '/'; ++nextname);
			
			/*If the filename ends in a '/'*/
			if(*nextname == 0)
				{
				/*reset `nextname`*/
				nextname = NULL;
				
				/*we have reached the last component of the path*/
				lastcomp = 1;
				
				/*a directory is required*/
				mustbedir = 1;
				
				/*nothing is to be created*/
				create = 0;
				}
			else
				/*we still haven't reached the last component of the path*/
				lastcomp = 0;
			}
		/*The current path contains no slashes any more*/
		else
			/*so, we are at the last component of the path now*/
			lastcomp = 1;
		
		/*`np` points to nothing at first*/
		np = NULL;
		
		/*perform the lookup*/
		do_lookup();
		
		/*If errors have occurred during the lookup*/
		if(err)
			{
			/*perform finalization and stop*/
			finalize();
			return err;
			}
			
		/*If the lookup has finished with no errors*/
		if(np)
			{
			/*validate the stat information for `np`*/
			mutex_lock(&np->lock);
			err = netfs_validate_stat(np, diruser->user);
			mutex_unlock(&np->lock);
			
			/*stop if validation failed*/
			if(err)
				{
				finalize();
				return err;
				}
			
			/*If `np` refers to a symlink, and if we are not at the last component
				of the path, or if we are asked to lookup a directory or if we are not
				required to ignore symlinks and translators*/
			if
				(
				S_ISLNK(np->nn_translated)
				&& (!lastcomp || mustbedir || !(flags & (O_NOLINK | O_NOTRANS)))
				)
				{
				/*If the number of followed symlinks was exceeded*/
				if(nsymlinks++ > netfs_maxsymlinks)
					{
					/*stop*/
					err = ELOOP;
					finalize();
					return err;
					}

				/*obtain the length of `nextname`*/
				size_t nextnamelen = (nextname) ? (strlen(nextname) + 1) : (0);
				
				/*the length of the stat information about the link we are at*/
				/*although it looks like the length of the path to the target of
					the symlink*/
				size_t linklen = np->nn_stat.st_size;
				
				/*The idea is that we are at a symlink, we substitute the
					path we are currently at with the path to the target
					of the symlink (dereference it).*/
				
				/*the length the current component of the path together with the stat
					information about it*/
				/*or the length of the current component together with the length
					of the path to the target of the symlink*/
				size_t newnamelen = nextnamelen + linklen + 1;
				
				/*allocate some space on the stack for information about symlink*/
				char * linkbuf = alloca(newnamelen);
				
				/*try to read the information about the symlink*/
				/*obtain the path to the target of the symlink*/
				err = netfs_attempt_readlink(diruser->user, np, linkbuf);
				if(err)
					{
					finalize();
					return err;
					}
				
				/*If the current component of the path is not empty*/
				if(nextname)
					{
					/*add a slash to end of the obtained path*/
					linkbuf[linklen] = '/';
					
					/*add the current component of the path into `linkbuf`*/
					strncpy(linkbuf + linklen + 1, nextname, nextnamelen - 1);
					}
				/*add a terminal 0 to the end of `linkbuf`*/
				linkbuf[newnamelen - 1] = 0;
				
				/*If the new path is absolute*/
				if(linkbuf[0] == '/')
					{
					/*tell the caller they should retry the new path in `linkbuf`*/
					*do_retry = FS_RETRY_MAGICAL;
					*retry_port = MACH_PORT_NULL;
					strcpy(retry_name, linkbuf);
					
					/*stop*/
					finalize();
					return err;
					}
				
				/*update the filename*/
				filename = linkbuf;
				
				/*If we are at the last component of the path*/
				if(lastcomp)
					{
					/*forget that we are at the last component*/
					lastcomp = 0;

					/*prohibit creation, if the symlink points to a nonexistent file*/
					create = 0;
					}
				
				/*release `np`*/
				netfs_nput(np);
				np = NULL;
				
				/*lock the directory*/
				mutex_lock(&dnp->lock);
				}
			/*If the node does not refer to a symlink*/
			else
				{
				/*update the filename*/
				filename = nextname;
				
				/*release the node referring to the directory*/
				netfs_nrele(dnp);
				
				/*If we are at the last component of the path*/
				if(lastcomp)
					/*drop dnp*/
					dnp = NULL;
				else
					{
					/*move `dnp` to `np`*/
					dnp = np;
					}
				}
			}
		}
	while(filename && *filename);
	
	/*Return `np`*/
	return_np();
	return err;
	}/*netfs_S_dir_lookup*/
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
	return 0;
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
	return 0;
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
	return 0;
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
	/*Operation not supported (why?..)*/
	return EOPNOTSUPP;
	}/*netfs_attempt_readlink*/
/*----------------------------------------------------------------------------*/
/*Reads from file `node` up to `len` bytes from `offset` into `data`*/
error_t
netfs_attempt_read
	(
	struct iouser * cred,
	struct node * node,
	loff_t offset,
	size_t * len,
	void * data
	)
	{
	return 0;
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
	return 0;
	}/*netfs_attempt_write*/
/*----------------------------------------------------------------------------*/
/*Frees all storage associated with the node*/
void
netfs_node_norefs
	(
	struct node * node
	)
	{
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
		
	/*Stop logging*/
	LOG_MSG(">> Successfully went away.");
	CLOSE_LOG();
	}/*main*/
/*----------------------------------------------------------------------------*/
