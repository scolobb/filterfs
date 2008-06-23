/*----------------------------------------------------------------------------*/
/*filter.c*/
/*----------------------------------------------------------------------------*/
/*The filtering translator*/
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
#include "node.h"
#include "options.h"
#include "ncache.h"
#include "lib.h"
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
	return 0;
	}/*netfs_attempt_create_file*/
/*----------------------------------------------------------------------------*/
/*Return an error if the process of opening a file should not be allowed
	to complete because of insufficient permissions*/
error_t
netfs_check_open_permissions
	(
	struct iouser * user,
	struct node * node,
	int flags,
	int newnode
	)
	{
	return 0;
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
	struct node * node,
	struct iouser * cred
	)
	{
	return 0;
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
		ino_t fileno,
		int type
		)
		{
		/*If the required number of dirents has not been listed yet*/
		if((num_entries == -1) && (count < num_entries))
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
			hdr.d_fileno = fileno;
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
			)
			
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
/*Looks up `name` in `dir` for `user`*/
error_t
netfs_attempt_lookup
	(
	struct iouser * user,
	struct node * dir,
	char * name,
	struct node ** node
	)
	{
	return 0;
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
	return 0;
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
	/*The port on which this translator will be set upon*/
	mach_port_t bootstrap_port;
	
	error_t err = 0;
	
	/*Parse the command line arguments*/
	argp_parse(&argp_startup, argc, argv, ARGP_IN_ORDER, 0, 0);
	
	/*Try to create the root node*/
	err = node_create_root(&netfs_root_node);
	if(err)
		error(EXIT_FAILURE, err, "Failed to create the root node");

	/*Obtain the bootstrap port*/
	task_get_bootstrap_port(mach_task_self(), &bootstrap_port);

	/*Initialize the translator*/
	netfs_init();
	
	/*Obtain a port to the underlying node*/
	underlying_node = netfs_startup(bootstrap_port, O_READ);

	/*Initialize the root node*/
	err = node_init_root(netfs_root_node);
	if(err)
		error(EXIT_FAILURE, err, "Failed to initialize the root node");
	
	/*Map the time for updating node information*/
	err = maptime_map(0, 0, &maptime);
	if(err)
		error(EXIT_FAILURE, err, "Failed to map the time");
		
	/*Initialize the cache with the required number of nodes*/
	ncache_init(ncache_size);
	
	/*Obtain stat information about the underlying node*/
	err = io_stat(underlying_node, &underlying_node_stat);
	if(err)
		error(EXIT_FAILURE, err, "Cannot obtain stat information about the underlying node");
		
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
	
	/*Start serving clients*/
	for(;;)
		netfs_server_loop();
	}/*main*/
/*----------------------------------------------------------------------------*/
