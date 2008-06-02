/*----------------------------------------------------------------------------*/
/*lnode.h*/
/*----------------------------------------------------------------------------*/
/*Management of cheap 'light nodes'*/
/*----------------------------------------------------------------------------*/
#ifndef __LNODE_H__
#define __LNODE_H__

/*----------------------------------------------------------------------------*/
#include <error.h>
#include <hurd/netfs.h>
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*A candy synonym for the fundamental libnetfs node*/
typedef struct node node_t;
/*----------------------------------------------------------------------------*/
/*The light node*/
struct lnode
	{
	/*the name of the lnode*/
	char * name;
	
	/*the length of the name; `name` does not change, and this value is used
	quite often, therefore calculate it just once*/
	size_t name_len;
	
	/*the associated flags*/
	int flags;
	
	/*the number of references to this lnode*/
	int references;
	
	/*the reference to the real node*/
	node_t * node;
	
	/*the next lnode and the pointer to this lnode from the previous one*/
	struct lnode * next, **prevp;
	
	/*the lnode (directory) in which this node is contained*/
	struct lnode * dir;
	
	/*the beginning of the list of entries contained in this lnode (directory)*/
	struct lnode * entries;
	
	/*a lock*/
	struct mutex lock;
	};/*struct lnode*/
/*----------------------------------------------------------------------------*/
typedef struct lnode lnode_t;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Adds a reference to the `lnode` (which must be locked)*/
void
lnode_ref_add
	(
	lnode_t * node
	);
/*----------------------------------------------------------------------------*/
#endif
