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
#include <argp.h>
#include <error.h>
#include <argz.h>
#include <hurd/netfs.h>
/*----------------------------------------------------------------------------*/
#include "node.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The name of the server*/
char * netfs_server_name = "filterfs";
/*----------------------------------------------------------------------------*/
/*The version of the server*/
char * netfs_server_version = "0.0";
/*----------------------------------------------------------------------------*/
/*The version of the server for argp*/
const char * argp_program_version = "0.0";
/*----------------------------------------------------------------------------*/
/*Short documentation for argp*/
static char * args_doc = "FILTERFS";
static char * doc = "Filtering translator.";
/*----------------------------------------------------------------------------*/
/*The maximal length of a chain of symbolic links*/
int netfs_maxsymlinks = 12;
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
	return 0;
	}/*netfs_attempt_sync*/
/*----------------------------------------------------------------------------*/
/*Fetches a directory*/
error_t
netfs_get_dirents
	(
	struct iouser * cred,
	struct node * dir,
	int first_entry,
	int max_entries,
	char ** data,
	mach_msg_type_number_t * data_len,
	vm_size_t max_data_len,
	int * data_entries
	)
	{
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	return 0;
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
	
	/*Try to create the root node*/
	err = node_create_root(&netfs_root_node);
	if(err)
		error(EXIT_FAILURE, err, "Failed to create the root node");
	
	/*Return success*/
	return 0;
	}/*main*/
/*----------------------------------------------------------------------------*/
