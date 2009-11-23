/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * The actual worker thread method and associated functions
 *
 */

#include <stdio.h>              /* sadly, some printf statements */
#include <pthread.h>            /* mutexes, conditions */
#include <string.h>             /* strlen() */

#include "parcle.h"


#ifdef HAVE_LUA
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#endif


#ifdef HAVE_LUA
static int      l_buffer_output        ( lua_State *L );
static int      l_get_output_buffer    ( lua_State *L );

/* set up the Lua bindings for C-functions */
const struct luaL_reg app_lib [] = {
	{"commit",   l_buffer_output},
	{"prepare",  l_get_output_buffer},
	{NULL,       NULL}
};
#else
static void c_response                 ( struct cn_strct *cn);
#endif


/*
 * Run the actual application workers, just keep looping through them and see if
 * something is left to do
 */
void
*run_app_thread (void *tid)
{
	struct cn_strct *cn;
	int              id =       *((int*) tid);
	int              sent;

#ifdef HAVE_LUA
	// thread local lua state
	lua_State *L = lua_open();
	luaL_openlibs (L);
	luaL_openlib  (L, "parcle", app_lib, 0);
	luaL_dofile   (L, "app/_init.lua");
#endif

	while(1) {
		// monitor
		pthread_mutex_lock( &wake_worker_mutex );
		while (NULL == _Queue_head) {
			pthread_cond_wait( &wake_worker_cond, &wake_worker_mutex );
		}
		pthread_mutex_unlock( &wake_worker_mutex );

		/* pull job from queue */
		pthread_mutex_lock   ( &pull_job_mutex );
		if (NULL == _Queue_head) {
			printf("QUEUE MISSED!!\n");
			pthread_mutex_unlock ( &pull_job_mutex );
			continue;
		}
		else {
			cn   =   _Queue_head;
			_Queue_count--;
			if (NULL == _Queue_head->q_prev) {
				_Queue_head = NULL;
				_Queue_tail = NULL;
			}
			else {
				_Queue_head = cn->q_prev;
				cn->q_prev  = NULL;
			}
		}
#if DEBUG_VERBOSE == 3
		if (_Queue_count > 1)
			printf("Left in Queue AFTER REMOVAL: %d\n",
				_Queue_count
			);
#endif
		pthread_mutex_unlock ( &pull_job_mutex );

#ifdef HAVE_LUA
		/* Execute the lua function we want */
		lua_getglobal(L, "test");
		lua_pushlightuserdata(L, (void*) cn);
		lua_call(L, 1, 0);
#else
		c_response(cn);
		cn->out_buf         = cn->data_buf_head;
		cn->processed_bytes = strlen(cn->data_buf_head);
#endif

		/* signal the select loop that we are done ... */
		while (REQSTATE_SEND_FILE != cn->req_state) {
			sent = send (cn->net_socket, "", 0, 0);
			cn->req_state       = REQSTATE_SEND_FILE;
		}

		/* pick up some slack in case some others missed */
		pthread_cond_signal (&wake_worker_cond);
	}
}



#ifdef HAVE_LUA

/* TODO: the actual part of the Lua lib -> factor out */
static int
l_buffer_output (lua_State *L)
{
	struct cn_strct *cn  = NULL;
	cn  =  (struct cn_strct*) lua_touserdata(L, 1);

	cn->processed_bytes = lua_strlen (L, 2);
	strncpy( cn->data_buf_head, lua_tostring (L, 2), cn->processed_bytes );

	return 0;
}

static int
l_get_output_buffer (lua_State *L)
{
	struct cn_strct *cn  = NULL;
	cn                   =  (struct cn_strct*) lua_touserdata(L, 1);

	cn->processed_bytes = lua_strlen (L, 2);
	cn->out_buf = lua_tolstring (L, 2, &cn->processed_bytes);

	return 0;
}
#else
/*
 * A native (dummy) method that returns a static response into the connections
 * socket. It's meant to be used as a test method only
 */
static void
c_response ( struct cn_strct *cn )
{
	char *page = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
  \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\" >\n\
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\" >\n\
<head>\n\
  <title>A C-generated dynamic page</title>\n\
  <meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\n\
</head>\n\
<body>\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />\n\
</body>\n\
</html>\n";
	cn->processed_bytes = snprintf(
		cn->data_buf_head, RECV_BUFF_LENGTH,
		HTTP_VERSION" 200 OK\r\n"
		"Server: %s\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: %Zd\r\n"
		"Date: %s\r\n"
		"Last-Modified: %s\r\n\r\n%s"
		, _Server_version, strlen(page),
		_Master_date, _Master_date, page
	);
}
#endif
