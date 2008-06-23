/*----------------------------------------------------------------------------*/
/*node.h*/
/*----------------------------------------------------------------------------*/
/*Node management. Also see lnode.h*/
/*----------------------------------------------------------------------------*/
#ifndef __NODE_H__
#define __NODE_H__

/*----------------------------------------------------------------------------*/
#include <error.h>
#include <sys/stat.h>
#include <hurd/netfs.h>
/*----------------------------------------------------------------------------*/
#include "lnode.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*The user-defined node for libnetfs*/
struct netnode
	{
	/*the reference to the corresponding light node*/
	lnode_t * lnode;
	
	/*the flags associated with this node (might be not required)*/
	int flags;

	/*a port to the underlying filesystem*/
	file_t port;
	
	/*the neighbouring entries in the cache*/
	node_t * ncache_prev, * ncache_next;
	};/*struct netnode*/
/*----------------------------------------------------------------------------*/
typedef struct netnode netnode_t;
/*----------------------------------------------------------------------------*/
/*A list element containing directory entry*/
struct node_dirent
	{
	/*the directory entry*/
	struct dirent * dirent;
	
	/*the next element*/
	struct node_dirent * next;
	};/*struct node_dirent*/
/*----------------------------------------------------------------------------*/
typedef struct node_dirent node_dirent_t;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The lock protecting the underlying filesystem*/
extern struct mutex ulfs_lock;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Derives a new node from `lnode` and adds a reference to `lnode`*/
error_t
node_create
	(
	lnode_t * lnode,
	node_t ** node	/*store the result here*/
	);
/*----------------------------------------------------------------------------*/
/*Creates the root node and the corresponding lnode*/
error_t
node_create_root
	(
	node_t ** root_node	/*store the result here*/
	);
/*----------------------------------------------------------------------------*/
/*Initializes the port to the underlying filesystem for the root node*/
error_t
node_init_root
	(
	node_t * node	/*the root node*/
	);
/*----------------------------------------------------------------------------*/
/*Frees a list of dirents*/
void
node_entries_free
	(
	node_dirent_t * dirents	/*free this*/
	);
/*----------------------------------------------------------------------------*/
/*Reads the directory entries from `node`, which must be locked*/
error_t
node_entries_get
	(
	node_t * node,
	node_dirent_t ** dirents /*store the result here*/
	);
/*----------------------------------------------------------------------------*/
#endif /*__NODE_H__*/
