/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice in parcle.h
 *
 * The start up of the server, error out handlicng and signals
 *
 */

#include <stdio.h>              /* sadly, some printf statements */
#include <signal.h>             /* catch Ctrl+C */
#include <stdlib.h>             /* exit() */
#include <string.h>             /* memset(), */
#include <fcntl.h>              /* F_GETFL, ... */
#include <pthread.h>            /* create the pool */

/* network, sockets, accept, IP handling */
#include <sys/socket.h>         /* _Master_sock */
#include <netinet/in.h>         /* open the connection and bind*/
#include <netdb.h>
#include <arpa/inet.h>

/*
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
*/

#include "parcle.h"
#include "utils.h"              /* pow2() */
/* ######################## GLOBAL VARIABLES ############################### */
struct cn_strct     *_Free_conns;       /* idleing conns, LIFO stack */
int                  _Free_count;
struct cn_strct     *_Busy_conns;       /* working conns, doubly linked list */
int                  _Busy_count;
struct cn_strct*    *_All_conns;        /* all conns, indexed in array */
int                  _Master_sock;      /* listening master socket */
time_t               _Last_loop;        /* marks the last run of select */
char                 _Master_date[30];  /* the formatted date */
int                  _Conn_count;       /* all existing cn_structs */
int                  _Conn_size;        /* 2^x connections */

/* a FIFO stack for quead up conns waiting for threads */
struct cn_strct     *_Queue_head;
struct cn_strct     *_Queue_tail;
int                  _Queue_count;
struct thread_arg    _Workers[WORKER_THREADS]; /* used to clean up */


static int   create_listener  ( int port );
static void  start_server     ( char *app_root, int port );

/* clean up after Ctrl+C or shutdown */
static void
clean_on_quit(int sig)
{
	struct cn_strct *tp;
	int i;
#if DEBUG_VERBOSE == 2
	printf("\n\n\n\n\nPRINTING QUEUE: \n");
	print_queue(_Queue_head, _Queue_count);
	printf("PRINTING QUEUE_LIST: \n");
	print_list(_Queue_tail);
	printf("PRINTING FREEs: \n");
	print_list(_Free_conns);
	printf("PRINTING BUSYs: \n");
	print_list(_Busy_conns);
#endif

	while (NULL != _Free_conns) {
		tp = _Free_conns->c_next;
		free(_Free_conns->data_buf_head);
		free(_Free_conns);
		_Free_conns = tp;
	}

	while (NULL != _Busy_conns) {
		tp = _Busy_conns->c_next;
#if DEBUG_VERBOSE == 2
		print_cn(tp);
#endif
		free(_Busy_conns->data_buf_head);
		close(_Busy_conns->net_socket);
		free(_Busy_conns);
		_Busy_conns = tp;
	}
	close(_Master_sock);
	_Master_sock = -1;
	printf("Graceful exit done after signal: %d\n", sig);

	/* cleanup the threads */
	for (i = 0; i < WORKER_THREADS; i++) {
		pthread_cancel(_Workers[i].thread);
	}

	exit(0);
}

static void
die(int sig)
{
	printf("Server stopped, caught signal: %d\n", sig);
	exit(0);
}


int
main(int argc, char *argv[])
{
	char    *command;

	command = argv[1];
	/* TODO: do some error handling here */
	if (1 < argc && 0 == strncmp(command, "server",  6)) {
		switch (argc) {
			case 4:
				start_server(argv[2], (int) strtol(argv[3],NULL,10));
				break;
			case 3:
				start_server(argv[2], APP_PORT);
				break;
			default:
				start_server(APP_ROOT, APP_PORT);
		}
	}
	else if (1 < argc && 0 == strncmp(command, "shell",  5)) {
		printf("Shell functionality is not yet implemented\n");
	}
	else {
		start_server(APP_ROOT, APP_PORT);
	}
	return 0;
}

static void
start_server( char *app_root, int port )
{
	struct cn_strct  *tp;
	int               i,f,r;
	struct tm        *tm_struct;
	int               pipe_set[2];

	_Conn_size = INIT_CONNS;

	/* initialize the masterdate we update only every second */
	_Last_loop = time(NULL);
	tm_struct  = gmtime(&_Last_loop);
	strftime( _Master_date, 32, "%a, %d %b %Y %H:%M:%S %Z", tm_struct);
	_Conn_count=0;
#if DEBUG_VERBOSE == 1
	printf("STARTED AT: %s\n", _Master_date);
#endif

	signal(SIGQUIT, die);
	signal(SIGTERM, die);
	signal(SIGINT, clean_on_quit);

	/* enter the directory with the application */
	if (chdir(app_root))
		clean_on_quit(2);

	/* Fill up the initial connection lists; _Free_conns is just a LIFO stack,
	 * there shall never be a performance issues -> single linked only */
	_Free_count=0;
	_Busy_count=0;
	_All_conns = (struct cn_strct **)
		malloc (pow2(_Conn_size) * sizeof( struct cn_strct *));

	for (i = 0; i < pow2(_Conn_size); i++) {
		tp = _Free_conns;
		_Free_conns = (struct cn_strct *) calloc(1, sizeof(struct cn_strct));
		_Free_conns->data_buf_head =
			(char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
		_Free_conns->c_next = tp;
		_Free_conns->c_prev = NULL;
		_Free_conns->q_prev = NULL;
		_Free_conns->id     = _Conn_count;
		_All_conns[_Conn_count] = _Free_conns;
		_Free_count++;
		_Conn_count++;
	}

	/* create the master listener */
	if ((_Master_sock = create_listener(port)) == -1) {
		fprintf(stderr, "ERR: Couldn't bind to port %d\n",
				port);
		exit(1);
	}

	/* set up LIFO queue */
	_Queue_tail = _Queue_head = NULL;
	_Queue_count = 0;

	/* create workers for application */
	for(i = 0; i < WORKER_THREADS; i++) {
		pipe(pipe_set);
		_Workers[i].r_pipe = pipe_set[0];
		_Workers[i].w_pipe = pipe_set[1];
		_Workers[i].t_id   = i;
		f = fcntl(pipe_set[0], F_GETFL, 0);
		r = fcntl(pipe_set[0], F_SETFL, f | O_NONBLOCK);
		f = fcntl(pipe_set[1], F_GETFL, 0);
		r = fcntl(pipe_set[1], F_SETFL, f | O_NONBLOCK);
		pthread_create(&_Workers[i].thread,
			NULL,
			&run_app_thread,
			(void *) &_Workers[i]
		);
	}
	sleep(1);
	for(i = 0; i < WORKER_THREADS; i++) {
		pthread_detach( _Workers[i].thread );
	}

#if DEBUG_VERBOSE == 1
	printf("%s: listening on port %d (http)\n",
			_Server_version, port);
#endif

	/* Kick it off */
	server_loop();
}

/* create the master listening socket */
static int
create_listener(int port)
{
	int                 tmp_s=0, sd;
	struct sockaddr_in  my_addr;

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family      = AF_INET;
	my_addr.sin_port        = htons((short)port);
	my_addr.sin_addr.s_addr = INADDR_ANY;

	if (0 > setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &tmp_s, sizeof(int)) ) {
		printf("Failed to reuse the listener socket\n");
		close(sd);
		return -1;
	}
	if (bind(sd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		close(sd);
		return -1;
	}

	listen(sd, BACK_LOG);
	return sd;
}

#if DEBUG_VERBOSE == 2
/*
 * a simplistic way to print out linked lists, a very crude visualization but it
 * helps debugging
 */
void
print_list (struct cn_strct *nd)
{
	struct cn_strct *tmp, *tmp1;
	int              cnt = 0;

	tmp=nd;
	printf( "prev\tdata\tnext\n" );
	while (NULL != tmp) {
		if (tmp == nd && 0 < cnt++) {
			printf("DETECTED LOOP \n");
			break;
		}
		tmp1 = tmp->c_next;
		if (NULL != tmp->c_prev && NULL != tmp->c_next)
			printf("%d\t%d\t%d\n", tmp->c_prev->id,
				tmp->id, tmp->c_next->id );
		else if (NULL == tmp->c_prev && NULL != tmp->c_next)
			printf("  \t%d\t%d\n", tmp->id, tmp->c_next->id );
		else if (NULL != tmp->c_prev && NULL == tmp->c_next)
			printf("%d\t%d\t  \n", tmp->c_prev->id, tmp->id);
		else
			printf("  \t%d\t  \n", tmp->id);
		tmp=tmp1;
	}
}

void
print_queue (struct cn_strct *nd, int count)
{
	struct cn_strct *tmp, *tmp1;
	int              cnt = 0;

	tmp=nd;
	printf( "q_prev\tdata\tq_next\n" );
	while (NULL != tmp) {
		if (tmp == nd && 0 < cnt) {
			printf("DETECTED LOOP \n");
			break;
		}
		tmp1 = tmp->q_prev;
		if (NULL == tmp->q_prev)
			printf("  \t%d\t  \t%d\n", tmp->id, count);
		else
			printf("%d\t%d\t  \t%d\n", tmp->q_prev->id, tmp->id, count);
		tmp=tmp1;
		cnt++;
	}
}
void
print_cn (struct cn_strct *cn)
{
	printf("\n\nIDENTIFIER:  %d\t"
		"REQSTATE:  %d\t"
		//"DATA_ALL: %s\n"
		//"DATA_NOW: %s\n"
		"PROCESSED: %d\n",
		cn->id,
		cn->req_state,
		//cn->data_buf_head,
		//cn->data_buf,
		cn->processed_bytes);
}
#endif

// vim: ts=4 sw=4 sts=4 sta tw=80 list
