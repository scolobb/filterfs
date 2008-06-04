/*----------------------------------------------------------------------------*/
/*options.h*/
/*----------------------------------------------------------------------------*/
/*Definitions for parsing the command line switches*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#define _GNU_SOURCE 1
/*----------------------------------------------------------------------------*/
#include <argp.h>
#include <error.h>
/*----------------------------------------------------------------------------*/
#include "options.h"
#include "ncache.h"
#include "node.h"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*This variable is set to a non-zero value after the parsing of starup options
	is finished*/
/*Whenever the argument parser is later called to modify the
	options of filterfs the root node will be initialized accordingly directly
	by the parser*/
static int parsing_startup_options_finished;
/*----------------------------------------------------------------------------*/
/*Argp options common to both the runtime and the startup parser*/
static const struct argp_option argp_common_options[] =
	{
	{OPT_LONG_CACHE_SIZE, OPT_CACHE_SIZE, "[size]", 0,
		"specify the maximal number of nodes in the node cache"},
	{OPT_LONG_PROPERTY, OPT_PROPERTY, "[property]", 0,
		"specify a command which will act as a filter"}
	};
/*----------------------------------------------------------------------------*/
/*Argp options only meaningful for startupp parsing*/
static const struct argp_option argp_startup_options[] =
	{
	{0}
	};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Functions-----------------------------------------------------------*/
/*Argp parser function for the common options*/
static
error_t
argp_parse_common_options
	(
	int key,
	char * arg,
	struct argp_state * state
	)
	{
	/*Go through the possible options*/
	switch(key)
		{
		case OPT_CACHE_SIZE:
			{
			/*store the new cache-size*/
			ncache_size = strtol(arg, NULL, 10);
			
			break;
			}
		case OPT_PROPERTY:
			{
			/*try to duplicate the filtering command*/
			property = strdup(arg);
			if(!property)
				error(EXIT_FAILURE, "Could not strdup the property");
				
			break;
			}
		}
	}/*argp_parse_common_options*/
/*----------------------------------------------------------------------------*/
