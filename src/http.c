/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * Parsing HTTP headers
 *
 */
#include <stdio.h>              /* sadly, some printf statements */
#include <string.h>             /* strncasecmp(), ... */ 

#include "parcle.h"

/* special cases served from the APP_ROOT directory */
#define FAVICON_URL           "/favicon"
#define FAVICON_URL_LENGTH    8
#define ROBOTS_URL            "/robots"
#define ROBOTS_URL_LENGTH     7

/*
 * * Isolate "METHOD URL?GET_PARAMS HTTP_VER" from first request line
 * - count '/' to help the url delimiter, count '?/ to get parser
 */
void
parse_first_line( struct cn_strct *cn )
{
	char          *next  = cn->data_buf_head;
	unsigned int   got_get=0, get_cnt=0, slash_cnt=0, error=0;
	/* METHOD */
	if      (0 == strncasecmp(next, "GET",     3)) { cn->req_type=REQTYPE_GET;     next+=3;}
	else if (0 == strncasecmp(next, "HEAD",    4)) { cn->req_type=REQTYPE_HEAD;    next+=4;}
	else if (0 == strncasecmp(next, "POST",    4)) { cn->req_type=REQTYPE_POST;    next+=4;}
	else if (0 == strncasecmp(next, "OPTIONS", 7)) { cn->req_type=REQTYPE_OPTIONS; next+=7;}
	else if (0 == strncasecmp(next, "DELETE",  6)) { cn->req_type=REQTYPE_DELETE;  next+=6;}
	else if (0 == strncasecmp(next, "PUT",     3)) { cn->req_type=REQTYPE_PUT;     next+=3;}
	*next = '\0';
	/* URL */
	next++;
	if ('/' == *next) {
		cn->url = next;
		if (0 == strncasecmp(cn->url, FAVICON_URL, FAVICON_URL_LENGTH ) ||
		    0 == strncasecmp(cn->url, ROBOTS_URL,  ROBOTS_URL_LENGTH )  ||
		    0 == strncasecmp(cn->url, STATIC_ROOT, STATIC_ROOT_LENGTH )) {
			cn->is_static = true;
		}
	}
	else {
		/* we are extremely unhappy ... -> malformed url
		   error(400, "URL has to start with a '/'!"); */
		printf("Crying game....\n");
	}
	/* chew through url, find GET, check url sanity */
	while ( !got_get && ' ' != *next ) {
		switch (*next) {
			case ' ':
				*next = '\0';
				break;
			case '?':
				got_get = 1;
				cn->get_str = next+1;
				*next = '\0';
				break;
			case '/':
				slash_cnt++;
				if ('.' == *(next+1) && '.' == *(next+2) && '/' == *(next+3))
					slash_cnt--;
				break;
			case '.':
				if ('/' == *(next-1) && '/' != *(next+1))
					error = 400;  /* trying to reach hidden files */
				break;
			default:
				/* keep chewing */
				break;
		}
		next++;
	}
	/* GET - count get parameters */
	while ( got_get && ' ' != *next ) {
		if( '=' == *next) {
			get_cnt++;
		}
		next++;
	}
	*next = '\0';
	next++;
	/* HTTP protocol version */
	if (0 == strncasecmp(next, "HTTP/", 5))
		next+=5;
	else
		// error(400, "The HTTP protocol type needs to be specified!");
		printf("Crying game....\n");
	if (0 == strncasecmp(next, "0.9", 3)) { cn->http_prot=HTTP_09; }
	if (0 == strncasecmp(next, "1.0", 3)) { cn->http_prot=HTTP_10; }
	if (0 == strncasecmp(next, "1.1", 3)) { cn->http_prot=HTTP_11; }
#if DEBUG_VERBOSE==1
	printf("URL SLASHES: %d -- GET PARAMTERS: %d --ERRORS: %d\n",
		slash_cnt, get_cnt, error);
#endif
}

