/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * Partial and dirty implementation of WSAPI in parcle
 *
 */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


/* the wrapper struct for the cn_struct */
struct request_env {
	struct cn_strct        *cn; /* the connection we wrap */
};
int l_register_request     ( lua_State *L );
