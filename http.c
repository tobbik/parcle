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
	if (0 == strncasecmp(next, "GET",  3)) { cn->req_type=REQTYPE_GET;  next+=3;}
	if (0 == strncasecmp(next, "HEAD", 4)) { cn->req_type=REQTYPE_HEAD; next+=4;}
	if (0 == strncasecmp(next, "POST", 4)) { cn->req_type=REQTYPE_POST; next+=4;}
	*next = '\0';
	/* URL */
	next++;
	if ('/' == *next) {
		cn->url = next;
	}
	else {
		// we are extremely unhappy ... -> malformed url
		// error(400, "URL has to start with a '/'!");
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
				cn->pay_load = next+1;
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
				// keep chewing
				break;
		}
		next++;
	}
	/* GET - count get parameters */
	while ( got_get && ' ' != *next ) {
		switch (*next) {
			case '=':
				get_cnt++;
				break;
			default:
				// keep chewing
				break;
		}
		next++;
	}
	*next = '\0';
	next++;
	/* HTTP protocol version */
	if (0 == strncasecmp(next, "HTTP/0.9", 8)) { cn->http_prot=HTTP_09; }
	if (0 == strncasecmp(next, "HTTP/1.0", 8)) { cn->http_prot=HTTP_10; }
	if (0 == strncasecmp(next, "HTTP/1.1", 8)) { cn->http_prot=HTTP_11; }
#if DEBUG_VERBOSE==1
	printf("URL SLASHES: %d -- GET PARAMTERS: %d --ERRORS: %d\n",
		slash_cnt, get_cnt, error);
#endif
}

