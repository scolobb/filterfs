/*----------------------------------------------------------------------------*/
/*options.h*/
/*----------------------------------------------------------------------------*/
/*Declarations for parsing the command line switches*/
/*----------------------------------------------------------------------------*/
#ifndef __OPTIONS_H__
#define __OPTIONS_H__

/*----------------------------------------------------------------------------*/
/*--------Macros--------------------------------------------------------------*/
/*The possible short options*/
#define OPT_CACHE_SIZE 'c'
/*the property according to which filtering will be performed*/
#define OPT_PROPERTY	 'p'
/*----------------------------------------------------------------------------*/
/*The corresponding long options*/
#define OPT_LONG_CACHE_SIZE "cache-size"
#define OPT_LONG_PROPERTY 	"property"
/*----------------------------------------------------------------------------*/
/*Makes a long option out of option name*/
#define OPT_LONG(o) "--" o
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*--------Global Variables----------------------------------------------------*/
/*The argp parser for startup arguments*/
extern struct argp argp_startup;
/*----------------------------------------------------------------------------*/
/*The argp parser for rutime arguments*/
extern struct argp argp_runtime;
/*----------------------------------------------------------------------------*/
/*The filtering command*/
char * property = NULL;
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
	);
/*----------------------------------------------------------------------------*/
#endif
