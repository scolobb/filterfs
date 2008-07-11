/*----------------------------------------------------------------------------*/
/*filter.h*/
/*----------------------------------------------------------------------------*/
/*The definitions for the filtering translator*/
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
#ifndef __FILTERFS_H__
#define __FILTERFS_H__
/*----------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdlib.h>
#include <cthreads.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <hurd/ihash.h>
#include <hurd/iohelp.h>
/*----------------------------------------------------------------------------*/
#include "lib.h"
#include "node.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*The inode for the root node*/
#define FILTERFS_ROOT_INODE 1
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*A mapped time value for filterfs*/
/*Required for a very fast access to time*/
extern volatile struct mapped_time_value * maptime;
/*----------------------------------------------------------------------------*/
/*A port to the underlying node*/
extern mach_port_t underlying_node;
/*----------------------------------------------------------------------------*/
/*The stat information about the underlying node*/
extern io_statbuf_t underlying_node_stat;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Structures----------------------------------------------------------*/
/*A single entry in a directory*/
struct filterfs_dir_entry
	{
	char * name;	/*name of the entry*/
	size_t hv;		/*hash value of NAME*/
	
	struct node * node; /*the active node referred to by this node (may be 0)*/
											/*NETFS_NDOE_REFCNT_LOCK should be hel whule frobbing this*/

	struct stat stat; /*status information*/

	char * symlink_target; /*the name of the node this entry might refer to*/
	
	time_t stat_timestamp; /*the moment at which the status information was retreieved*/
	
  /*the directory to which this entry belongs*/
	struct filterfs_dir * dir;
	
	/*link to the entry in hash bucket and the address of the previous entry's
	(or hashtable's) pointer to this entry*/
	/*If self_p is null, then this entry is deleted and awaiting
	the final disposal*/
	struct filterfs_dir_entry * next, ** self_p;
	
	/*the next and the previous entries in the 'directory order'; 0 if there
	are none*/
	struct filterfs_dir_entry * ordered_next, ** ordered_self_p;
	
	/*when the presence/absence of this file was last checked*/
	time_t name_timestamp;
	
	/*used for removing this entry*/
	hurd_ihash_locp_t inode_locp;
	
	/*a field for a negative result of lookup*/
	int noent : 1;
	
	/*a marker for gabage collecting*/
	int valid : 1;
	};/*struct filterfs_dir_entry*/
/*----------------------------------------------------------------------------*/
/*A directory*/
struct filterfs_dir
	{
	/*the number of entries in the hash table*/
	size_t num_entries;
	
	/*the number of entries that have nodes attached*/
	/*We keep an additional reference to our node if there is any,
		to prevent it from going away*/
	size_t num_live_entries;
	
	/*the hash table for entries*/
	struct filterfs_dir_entry ** htable;
	size_t htable_len; /*the lenght of the hash table*/
	
	/*the list of entries in 'directory order'.*/
	/*The entries are listed in a linked list using the ordered_next
		and ordered_self_p fields in each entry. Not all entries in htable
		need to be in this list*/
	struct filterfs_dir_entry * ordered;
	
	/*the filesystem node corresponding to this directory*/
	struct node * node;
	
	/*the filesystem this directory is in*/
	struct filterfs * fs;

	/*when the presence/absence of this file was last checked*/
	time_t name_timestamp;
	
	/*used for removing this entry*/
	hurd_ihash_locp_t inode_locp;
	
	/*Stuff for detecting bulk stats (?)*/
	/*Might be reduntant for this project*/

	/*the timestamp of the first sample in bulk_stat_count1, rounded to
		BULK_STAT_PERIOD seconds*/
	time_t buld_stat_base_stamp;
	
	/*the number of stats done in the period
	[bulk_stat_base_stamp, bulk_stat_base_stamp + BULK_STAT_PERIOD]*/
	unsigned bulk_stat_count_first_half;
	
	/*the number of stats done in the period
	[bulk_stat_bast_stamp + BULK_STAT_PERIOD,
		bulk_stat_bast_stamp + 2 * BULK_STAT_PERIOD]*/
	unsigned bulk_stat_count_second_half;
	};/*struct filterfs_dir*/
/*----------------------------------------------------------------------------*/
/*A particular filesystem*/
struct filterfs
	{
	/*the root of the filesystem*/
	struct node * root;
	
	/*inode numbers*/
	/*Assigned sequentially in the order of creation*/
	ino_t next_inode;
	
	/*the identifier of the filesystem (?)*/
	int fsid;
	
	/*a hash table mapping inode numbers to directory entries*/
	struct hurd_ihash inode_mappings;
	
	/*the lock for the hash table*/
	spin_lock_t inode_mappings_lock;
	};/*struct filterfs*/
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
	);
/*----------------------------------------------------------------------------*/
/*Returns an error if the process of opening a file should not be allowed
	to complete because of insufficient permissions*/
error_t
netfs_check_open_permissions
	(
	struct iouser * user,
	struct node * np,
	int flags,
	int newnode
	);
/*----------------------------------------------------------------------------*/
/*Attempts an utimes call for the user `cred` on node `node`*/
error_t
netfs_attempt_utimes
	(
	struct iouser * cred,
	struct node * node,
	struct timespec * atime,
	struct timespec * mtime
	);
/*----------------------------------------------------------------------------*/
/*Returns the valid access types for file `node` and user `cred`*/
error_t
netfs_report_access
	(
	struct iouser * cred,
	struct node * np,
	int * types
	);
/*----------------------------------------------------------------------------*/
/*Validates the stat data for the node*/
error_t
netfs_validate_stat
	(
	struct node * np,
	struct iouser * cred
	);
/*----------------------------------------------------------------------------*/
/*Syncs `node` completely to disk*/
error_t
netfs_attempt_sync
	(
	struct iouser * cred,
	struct node * node,
	int wait
	);
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
	);
/*----------------------------------------------------------------------------*/
/*Looks up `name` under `dir` for `user`*/
error_t
netfs_attempt_lookup
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	struct node ** node
	);
/*----------------------------------------------------------------------------*/
/*Deletes `name` in `dir` for `user`*/
error_t
netfs_attempt_unlink
	(
	struct iouser * user,
	struct node * dir,
	char * name
	);
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
	);
/*----------------------------------------------------------------------------*/
/*Attempts to create a new directory*/
error_t
netfs_attempt_mkdir
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	mode_t mode
	);
/*----------------------------------------------------------------------------*/
/*Attempts to remove directory `name` in `dir` for `user`*/
error_t
netfs_attempt_rmdir
	(
	struct iouser * user,
	struct node * dir,
	char * name
	);
/*----------------------------------------------------------------------------*/
/*Attempts to change the owner of `node` for user `cred` to `uid`:`gid`*/
error_t
netfs_attempt_chown
	(
	struct iouser * cred,
	struct node * node,
	uid_t uid,
	uid_t gid
	);
/*----------------------------------------------------------------------------*/
/*Attempts to change the author of `node` to `author`*/
error_t
netfs_attempt_chauthor
	(
	struct iouser * cred,
	struct node * node,
	uid_t author
	);
/*----------------------------------------------------------------------------*/
/*Attempts to change the mode of `node` to `mode` for `cred`*/
error_t
netfs_attempt_chmod
	(
	struct iouser * user,
	struct node * node,
	mode_t mode
	);
/*----------------------------------------------------------------------------*/
/*Attempts to turn `node` into a symlink targetting `name`*/
error_t
netfs_attempt_mksymlink
	(
	struct iouser * cred,
	struct node * node,
	char * name
	);
/*----------------------------------------------------------------------------*/
/*Attempts to turn `node` into a device; type can be either S_IFBLK or S_IFCHR*/
error_t
netfs_attempt_mkdev
	(
	struct iouser * cred,
	struct node * node,
	mode_t type,
	dev_t indexes
	);
/*----------------------------------------------------------------------------*/
/*Attempts to set the passive translator record for `file` passing `argz`*/
error_t
netfs_set_translator
	(
	struct iouser * cred,
	struct node * node,
	char * argz,
	size_t arglen
	);
/*----------------------------------------------------------------------------*/
/*Attempts to call chflags for `node`*/
error_t
netfs_attempt_chflags
	(
	struct iouser * cred,
	struct node * node,
	int flags
	);
/*----------------------------------------------------------------------------*/
/*Attempts to set the size of file `node`*/
error_t
netfs_attempt_set_size
	(
	struct iouser * cred,
	struct node * node,
	loff_t size
	);
/*----------------------------------------------------------------------------*/
/*Fetches the filesystem status information*/
error_t
netfs_attempt_statfs
	(
	struct iouser * cred,
	struct node * node,
	fsys_statfsbuf_t * st
	);
/*----------------------------------------------------------------------------*/
/*Syncs the filesystem*/
error_t
netfs_attempt_syncfs
	(
	struct iouser * cred,
	int wait
	);
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
	);
/*----------------------------------------------------------------------------*/
/*Attempts to create an anonymous file related to `dir` with `mode`*/
error_t
netfs_attempt_mkfile
	(
	struct iouser * user,
	struct node * dir,
	mode_t mode,
	struct node ** node
	);
/*----------------------------------------------------------------------------*/
/*Reads the contents of symlink `node` into `buf`*/
error_t
netfs_attempt_readlink
	(
	struct iouser * user,
	struct node * node,
	char * buf
	);
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
	);
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
	);
/*----------------------------------------------------------------------------*/
/*Frees all storage associated with the node*/
void
netfs_node_norefs
	(
	struct node * np
	);
/*----------------------------------------------------------------------------*/
#endif
