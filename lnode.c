/*----------------------------------------------------------------------------*/
/*lnode.c*/
/*----------------------------------------------------------------------------*/
/*Implementation of policies of management of 'light nodes'*/
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
#define _GNU_SOURCE
/*----------------------------------------------------------------------------*/
#include "lnode.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Adds a reference to the `lnode` (which must be locked)*/
void
lnode_ref_add
	(
	lnode_t * node
	)
	{
	/*Increment the number of references*/
	++node->references;
	}/*lnode_ref_add*/
/*----------------------------------------------------------------------------*/
/*Creates a new lnode with `name`; the new node is locked and contains
	a single reference*/
error_t
lnode_create
	(
	char * name,
	lnode_t ** node	/*put the result here*/
	)
	{
	/*Allocate the memory for the node*/
	lnode_t * node_new = malloc(sizeof(lnode_t));
	
	/*If the memory has not been allocated*/
	if(!node_new)
		{
		/*stop*/
		return ENOMEM;
		}
		
	/*The copy of the name*/
	char * name_cp = NULL;
	
	/*If the name exists*/
	if(name)
		{
		/*duplicate it*/
		name_cp = strdup(name);
		
		/*If the name has not been duplicated*/
		if(!name_cp)
			{
			/*free the node*/
			free(node_new);
			
			/*stop*/
			return ENOMEM;
			}
		}
		
	/*Setup the new node*/
	memset(node_new, 0, sizeof(lnode_t));
	node_new->name 				= name_cp;
	node_new->name_len		= (name_cp) ? (strlen(name_cp)) : (0);
	
	/*Setup one reference to this lnode*/
	node_new->references = 1;
	
	/*Initialize the mutex and acquire a lock on this lnode*/
	mutex_init(&node_new->lock);
	mutex_lock(&node_new->lock);
	
	/*Store the result in the second parameter*/
	*node = node_new;

	/*Return success*/
	return 0;
	}/*lnode_create*/
/*----------------------------------------------------------------------------*/
/*Destroys the given lnode*/
void
lnode_destroy
	(
	lnode_t * node	/*destroy this*/
	)
	{
	/*Destroy the name of the node*/
	free(node->name);
	
	/*Destroy the node itself*/
	free(node);
	}/*lnode_destroy*/
/*----------------------------------------------------------------------------*/
