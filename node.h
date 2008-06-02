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
	
	/*the neighbouring entries in the cache*/
	node_t * ncache_prev, * ncache_next;
	};/*struct netnode*/
/*----------------------------------------------------------------------------*/
typedef struct netnode netnode_t;
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
#endif
