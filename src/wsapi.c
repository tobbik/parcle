/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * Partial and dirty implementation of WSAPI in parcle
 *  - first, it's work in progress, try to stick at least to the naming
 *    conventions, then do what's best for performance and user convienience
 *  - wrap each cn_strct by a lua userdata struct request_env that can access
 *    the requested information on demand by utilizing the __index metamethod
 *  - using userdata enables both garbage collection and metamethods
 *  - instead of static objects we build them on demand, which also means, it's
 *    easier for the users to store them where they can mangle an customize them
 *
 */
#include <stdio.h>              /* sadly, some printf statements */
#include <string.h>             /* strncasecmp(), ... */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "parcle.h"
#include "wsapi.h"

/* internal forward declarations */
static int l_req_dispatch      ( lua_State *L );
static int l_req_immutable     ( lua_State *L );
static int l_req_size          ( lua_State *L, struct cn_strct *cn );
//static int l_req_get           ( lua_State *L, struct cn_strct *cn );
//static int l_req_method        ( lua_State *L, struct cn_strct *cn );
//static int l_req_query_string  ( lua_State *L, struct cn_strct *cn );
//static int l_req_path_info     ( lua_State *L, struct cn_strct *cn );
static int hexit               ( char c );
static int l_urldecode         ( lua_State *L, char *buf );

static const struct
luaL_reg l_request_lib [] = {
	{"dispatch",       l_req_dispatch},   /* call function depending to key */
	{"immutable",      l_req_immutable},  /* try adding to cn_struct, silly ? */
	{"size",           l_req_size},       /* what to put here ??? */
	{NULL,             NULL}
};



/* generate reusable metetable */
int
l_register_request (lua_State *L)
{
	// luaL_newmetatable(L, "wsapi_request");
	luaL_openlib(L, "request", l_request_lib, 0);

	// lua_pushstring(L, "__index");
	// lua_pushstring(L, "dispatch");
	// lua_gettable(L, 2);     // get request.dispatch
	// lua_settable(L, 1);     // metatable.__index = request.dispatch
	// 
	// lua_pushstring(L, "__newindex");
	// lua_pushstring(L, "immutable");
	// lua_gettable(L, 2);     // get request.dispatch
	// lua_settable(L, 1);     // metatable.__index = request.dispatch
	// 
	// lua_pushstring(L, "__len");
	// lua_pushstring(L, "size");
	// lua_gettable(L, 2);     // get array.size
	// lua_settable(L, 1);     // metatable.__len = array.size

	printf("REGISTERED LIB\n");

	return 1;
}

/* The request object cannot be written to */
static int
l_req_immutable (lua_State *L)
{
	luaL_error(L,
		"The request object is \"read-only\"!");
	return 1;
}

/*
 * @return: integer - whatever makes sense as len(req)
 * */
static int
l_req_size (lua_State *L, struct cn_strct *cn)
{
	lua_pushinteger(L, 111); // stupid static value for now
	return 1; /* one string or one nil value */
}


/*
 * @return: table - the GET parameters
 * */
static int
l_req_get (lua_State *L, struct cn_strct *cn)
{
	if (NULL == cn->get_str) {
		lua_pushnil(L);
	}
	else {
		lua_newtable(L);                     /* push GET table */
		lua_pushstring(L, "blah");      /* push value     */
		lua_rawseti(L, -2, 1);
	}
	return 1;   /* one table or one nil value */
}

/*
 * @return: string - the request method
 * */
static int
l_req_method (lua_State *L, struct cn_strct *cn)
{
	switch (cn->req_type) {
		case REQTYPE_POST:
			lua_pushstring(L, "POST");
			break;
		case REQTYPE_GET:
			lua_pushstring(L, "GET");
			break;
		case REQTYPE_HEAD:
			lua_pushstring(L, "HEAD");
			break;
		case REQTYPE_OPTIONS:
			lua_pushstring(L, "OPTIONS");
			break;
		case REQTYPE_DELETE:
			lua_pushstring(L, "DELETE");
			break;
		case REQTYPE_PUT:
			lua_pushstring(L, "PUT");
			break;
		default:
			lua_pushnil(L);
	}
	return 1; /* one string or one nil value */
}

/*
 * @return: string - the query string (after ?... )
 * */
static int
l_req_query_string (lua_State *L, struct cn_strct *cn)
{
	if ('\0' != cn->get_str)
		lua_pushstring(L, cn->get_str);
	else
		lua_pushnil(L);
	return 1; /* one string or one nil value */
}

/*
 * @return: string - the url (before ...? )
 * */
static int
l_req_path_info (lua_State *L, struct cn_strct *cn)
{
	if (NULL != cn->url)
		lua_pushstring(L, cn->url);
	else
		lua_pushnil(L);
	return 1; /* one string or one nil value */
}

/* a wrapper lua function that allows to return a url_param */
static int
l_req_dispatch (lua_State *L)
{
	struct request_env *re  = (struct request_env*) lua_touserdata(L, 1);
	const char         *key = luaL_checkstring(L, 2);
	if (0 == strncasecmp(key, "GET", 3)) {
		return l_req_method(L, re->cn);
	}
	else if (0 == strncasecmp(key, "method", 6)) {
		return l_req_method(L, re->cn);
	}
	else if (0 == strncasecmp(key, "query_string", 12)) {
		return l_req_query_string(L, re->cn);
	}
	else if (0 == strncasecmp(key, "path_info", 9)) {
		return l_req_path_info(L, re->cn);
	}

}

/*          _                            _
 _   _ _ __| |      _ __   __ _ _ __ ___(_)_ __   __ _
| | | | '__| |_____| '_ \ / _` | '__/ __| | '_ \ / _` |
| |_| | |  | |_____| |_) | (_| | |  \__ \ | | | | (_| |
 \__,_|_|  |_|     | .__/ \__,_|_|  |___/_|_| |_|\__, |
                   |_|                           |___/ */

/* found the urldecode in awhttpd, which in return said it came from zawhttpd
 * which disappeared from the face of the internet
 */
static int
hexit( char c )
{
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' )
		return c - 'A' + 10;
	return 0;
}

/* Decode string %xx -> char; from string into luaL_Buffer
 * @return:   the of the filled luaL_Buffer
 */
int
l_urldecode( lua_State *L, char *buf )
{
	short   v;
	size_t  c=0;
	char   *p,      /* main buffer*/
	       *s,      /**/
	       *w;      /**/

	w=p=buf;
	while (*p) {
		v=0;
		c++;

		if ('%'==*p) {
			s = p;
			s++;

			if ( isxdigit((int) s[0]) && isxdigit((int) s[1]) ) {
				v = hexit(s[0])*16+hexit(s[1]);
				if (v) {          /* don't decode %00 to \0 */
					*w = (char)v;
					p = &s[1];
				}
			}
		}
		if (!v) *w=*p;
		p++; w++;
	}
	*w='\0';

	return c;
}

