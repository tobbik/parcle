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
#include <unistd.h>             /* write */

#include "parcle.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static int      l_buffer_output        ( lua_State *L );
static int      l_get_output_buffer    ( lua_State *L );

/* set up the Lua bindings for C-functions */
const struct luaL_reg app_lib [] = {
	{"commit",   l_buffer_output},
	{"prepare",  l_get_output_buffer},
	{NULL,       NULL}
};

/*
 * Run the actual application workers, just keep looping through them and see if
 * something is left to do
 */
void
*run_app_thread (void *targs)
{
	struct thread_arg *args;
	struct cn_strct *cn;
	char             answer_buf[ANSWER_LENGTH];

	args = (struct thread_arg *) targs;

	/* thread local lua state */
	lua_State *L = lua_open();
	luaL_openlibs (L);
	luaL_openlib  (L, "parcle", app_lib, 0);
	if (luaL_loadfile(L, "app/_init.lua") || lua_pcall(L, 0, 0, 0))
		//error(L, "cannot run file: %s", lua_tostring(L, -1));
		;

	while(1) {
		/* monitor */
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

		/* Execute the lua function we want */
		lua_getglobal(L, "test");
		lua_pushlightuserdata(L, (void*) cn);
		lua_call(L, 1, 0);

		/* signal the select loop that we are done ...*/
		snprintf (answer_buf, ANSWER_LENGTH, "%d ", cn->id);
		write (args->w_pipe, answer_buf, strlen(answer_buf));

		/* pick up some slack in case some others missed */
		pthread_cond_signal (&wake_worker_cond);
	}
}


/* TODO: the actual part of the Lua lib -> factor out */
static int
l_buffer_output (lua_State *L)
{
	struct cn_strct *cn  = NULL;
	cn  =  (struct cn_strct*) lua_touserdata(L, 1);

	cn->processed_bytes = lua_strlen (L, 2);
	strncpy( cn->data_buf_head, lua_tostring (L, 2), cn->processed_bytes );
	cn->out_buf = cn->data_buf_head;

	return 0;
}

static int
l_get_output_buffer (lua_State *L)
{
	struct cn_strct *cn  = NULL;
	cn                   =  (struct cn_strct*) lua_touserdata(L, 1);

	cn->processed_bytes  = lua_strlen (L, 2);
	cn->out_buf = lua_tolstring (L, 2, &cn->processed_bytes);

	return 0;
}
