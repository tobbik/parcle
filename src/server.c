/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Copyright (C) 2009, Tobias Kieslich. All rights reseved.
 * See Copyright Notice at the end of the file
 *
 * This contains the main server loop and all the methods need to be called
 * depending on the different states.
 *
 */

#include <stdio.h>            /* sadly, some printf statements */
#include <sys/stat.h>         /* stat() */
#include <fcntl.h>            /* F_GETFL, ... */
#include <string.h>           /* memset() */
#include <stdlib.h>           /* calloc() */
#include <arpa/inet.h>        /* struct sockaddr_in */
#include <unistd.h>           /* read, close */

#include "parcle.h"
#include "utils.h"            /* pow2() */



/* connection helpers for main server_loop */
static void  handle_new_conn        ( int listenfd );
static void  add_conn_to_list       ( int sd, char *ip );
static void  remove_conn_from_list  ( struct cn_strct *cn );

/* select's processing helpers -> dispatched based on req_state */
static void  read_request           ( struct cn_strct *cn );
static void  write_head             ( struct cn_strct *cn );
static void  buff_file              ( struct cn_strct *cn );
static void  send_file              ( struct cn_strct *cn );

/* TODO: A helper, shall be factored out */
static const char *get_mime_type    ( const char *name );




void
server_loop()
{
	fd_set              rfds, wfds;
	struct cn_strct    *tp, *to;
	int                 rnum, wnum, readsocks, i, rp;
	char               *cn_id;
	char                answer[ANSWER_LENGTH];

	while (1) {
		// clean socket lists
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		wnum = -1;

		// Add master listener to reading sockets
		FD_SET(_Master_sock, &rfds);
		rnum = _Master_sock;

		// Add the established sockets
		tp = _Busy_conns;

		/* Adding connection to the SocketSets based on state */
		while (tp != NULL) {
			if (REQSTATE_READ_HEAD == tp->req_state) {
				FD_SET(tp->net_socket, &rfds);
				rnum = (tp->net_socket > rnum) ? tp->net_socket : rnum;
			}
			if (REQSTATE_SEND_HEAD == tp->req_state) {
				FD_SET(tp->net_socket, &wfds);
				wnum = (tp->net_socket > wnum) ? tp->net_socket : wnum;
			}
			if (REQSTATE_BUFF_FILE == tp->req_state) {
				FD_SET(tp->file_desc, &rfds);
				rnum = (tp->file_desc > rnum) ? tp->file_desc : rnum;
			}
			if (REQSTATE_SEND_FILE == tp->req_state) {
				FD_SET(tp->net_socket, &wfds);
				wnum = (tp->net_socket > wnum) ? tp->net_socket : wnum;
			}
			tp = tp->c_next;
		}
		/* Adding the ipc pipes */
		for (i=0; i<WORKER_THREADS; i++) {
			FD_SET (_Workers[i].r_pipe, &rfds);
			rnum = (_Workers[i].r_pipe > rnum) ? _Workers[i].r_pipe : rnum;
		}

		readsocks = select(
			(wnum > rnum) ? wnum+1 : rnum+1,
			(-1 != rnum)  ? &rfds : NULL,
			(-1 != wnum)  ? &wfds : NULL,
			(fd_set *) 0,
			NULL
		);

		// is the main listener in the read set? -> New connection
		if (FD_ISSET(_Master_sock, &rfds)) {
			handle_new_conn(_Master_sock);
			readsocks--;
		}

		// Handle the established sockets
		tp = _Busy_conns;

		while (readsocks > 0 && tp != NULL) {
			to = tp;
			tp = tp->c_next;

			if (REQSTATE_READ_HEAD == to->req_state &&
			  FD_ISSET(to->net_socket, &rfds)) {
				readsocks--;
#if DEBUG_VERBOSE == 1
				printf("WANNA RECV HEAD\n");
#endif
				read_request(to);
			}
			if (REQSTATE_SEND_HEAD == to->req_state &&
			  FD_ISSET(to->net_socket, &wfds)) {
				readsocks--;
#if DEBUG_VERBOSE == 1
				printf("WANNA SEND HEAD\n");
#endif
				write_head(to);
			}
			if (REQSTATE_BUFF_FILE == to->req_state &&
			  FD_ISSET(to->file_desc, &rfds)) {
				readsocks--;
#if DEBUG_VERBOSE == 1
				printf("WANNA BUFF FILE\n");
#endif
				buff_file(to);
			}
			if (REQSTATE_SEND_FILE == to->req_state &&
			  FD_ISSET(to->net_socket, &wfds)) {
				readsocks--;
#if DEBUG_VERBOSE == 1
				printf("WANNA SEND FILE\n");
#endif
				send_file(to);
			}
		}

		/* Has an app thread finished ? The put back in wfds */
		for (i=0; i<WORKER_THREADS; i++) {
			if (FD_ISSET (_Workers[i].r_pipe, &rfds) ) {
				readsocks--;
				rp = read (_Workers[i].r_pipe, answer, ANSWER_LENGTH);
				answer[rp] = '\0';
				// printf("ANSWER: %s --- ", answer);
				cn_id = strtok (answer, " ");
				while(cn_id != NULL) {
					_All_conns[ atoi (cn_id) ]->req_state = REQSTATE_SEND_FILE;
					cn_id = strtok (NULL, " ");
				}
				readsocks--;
				//printf("\n");
			}
		}
	}
}

/*
 * get a socket and form a cn_strct around it
 *  - either pull it of free_conns or create one
 *  - add it to the tail of _Busy_conns
 * */
static void
add_conn_to_list(int sd, char *ip)
{
	struct cn_strct *tp;

	/* pop a cn_strct from the free list ... or create one */
	if (NULL == _Free_conns) {
		if (pow2(_Conn_size) <= _Conn_count) {
			_Conn_size++;
			_All_conns = (struct cn_strct **)
				realloc (_All_conns,
					pow2(_Conn_size) * sizeof( struct cn_strct *)
				);
		}
		tp = (struct cn_strct *) calloc (1, sizeof(struct cn_strct));
		tp->data_buf_head = (char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
		_Free_count=0;
		tp->id = _Conn_count;
		_All_conns[_Conn_count] = tp;
		_Conn_count++;
	}
	else {
		tp = _Free_conns;
		_Free_conns = tp->c_next;
		/* TODO: For Later, if we end up reallocing for larger buffers we need
		 * to keep track of how much we need to null over upon reuse
		 */
		memset(tp->data_buf_head, 0, RECV_BUFF_LENGTH * sizeof(char));
		_Free_count--;
	}

	/* attach to tail of the _Busy_conns */
	if (NULL == _Busy_conns) {
		tp->c_next          = NULL;
		tp->c_prev          = NULL;
		_Busy_conns         = tp;
	}
	else {
		tp->c_next          = _Busy_conns;
		_Busy_conns->c_prev = tp;
		_Busy_conns         = tp;
	}
	_Busy_count++;
	tp->net_socket = sd;
	/* make sure the FIFO queue pointer is empty */
	tp->q_prev     = NULL;

	/* Pre/Re-set initial variables */
	tp->data_buf         = tp->data_buf_head;
	tp->req_state        = REQSTATE_READ_HEAD;
	tp->req_type         = REQTYPE_GET;
	tp->processed_bytes  = 0;
	tp->line_count       = 0;
	tp->get_str          = NULL;
	tp->is_static        = false;
	tp->url              = NULL;
}

static void
handle_new_conn( int listen_sd )
{
	struct sockaddr_in their_addr;
	socklen_t tp = sizeof(struct sockaddr_in);
	int connfd = accept(listen_sd, (struct sockaddr *)&their_addr, &tp);
	int x = fcntl(connfd, F_GETFL, 0);              /* Get socket flags */
	fcntl(connfd, F_SETFL, x | O_NONBLOCK);     /* Add non-blocking flag */
	add_conn_to_list(connfd, inet_ntoa(their_addr.sin_addr));
}

static void
remove_conn_from_list( struct cn_strct *cn )
{
	struct cn_strct *tp;

	tp = cn;

	if (tp == NULL || cn == NULL)
		return;

	if (NULL == tp->c_prev) {          /* tail of _Busy_conns */
		if (NULL == tp->c_next) {      /* only one in the list */
			_Busy_conns = NULL;
		}
		else {
			tp->c_next->c_prev  = NULL;
			_Busy_conns         = tp->c_next;
		}
		_Busy_count--;
	}
	else if (NULL == tp->c_next) {    /* head of _Busy_conns */
		tp->c_prev->c_next  = NULL;
		tp->c_prev          = NULL;
		_Busy_count--;
	}
	else {
		tp->c_prev->c_next = tp->c_next;
		tp->c_next->c_prev = tp->c_prev;
		_Busy_count--;
	}

	/* Attach to the end of the _Free_conns, only single link it with c_next */
	cn->c_next          = _Free_conns;
	cn->c_prev          = NULL;
	_Free_conns         = cn;
	_Free_count++;

	/* Close it all down */
	if (cn->net_socket != -1) {
		close(cn->net_socket);
	}
}

/* Here is the deal, we read as much as we can into the empty buffer, then
 * reset the buffer pointer to the end of the read material and append at
 * next read
 */
static void
read_request( struct cn_strct *cn )
{
	char *next;
	int   num_recv;

	/* FIXME: For now assume that RECV_BUFF_LENGTH is enough for one read */
	num_recv = recv(
		cn->net_socket,
		cn->data_buf,
		RECV_BUFF_LENGTH - cn->processed_bytes,
		//MAX_READ_LENGTH,
		0
	);

	/* sanity check - we can't read, just assume the worst */
	if (num_recv <= 0) {
		remove_conn_from_list(cn);
		return;
	}

	/* set the read pointer to where we left off */
	next = cn->data_buf_head + cn->processed_bytes;

	/* adjust buffer */
	//cn->processed_bytes += num_recv;
	cn->data_buf = cn->data_buf_head + cn->processed_bytes + num_recv;

	/* null terminate the current buffer -> overwrite on next read */
	cn->data_buf_head[cn->processed_bytes+num_recv] = '\0';

	/* a naive little line parser */
	while ( true ) {
		next = strchr( next+1, (int) '\r' );
		if (NULL == next) {
			cn->processed_bytes += num_recv;
			break;
		}
		else if ('\n' == *(next+1) ) {
			if (NULL == cn->url) {
				parse_first_line(cn);
			}
			if ( '\r'==*(next+2) && '\n'==*(next+3) ) {
				// proceed next stage
				cn->req_state = REQSTATE_SEND_HEAD;
				break;
			}
		}
	}
	// adjust buffer
	cn->processed_bytes += num_recv;
#if DEBUG_VERBOSE == 1
	if (REQSTATE_SEND_HEAD == cn->req_state) {
		printf("METHOD: %d\n",   cn->req_type);
		printf("URL: %s\n",      cn->url);
		printf("PROTOCOL: %d\n", cn->http_prot);
		printf("GET: %s\n",      cn->get_str);
	}
#endif
}

/* depending on if it is static or dynamic
 *  - static:  stat(file), send header, prepare for file buffering
 *  - dynamic: enqueue for thread pool, let it handle everything
 */
static void
write_head (struct cn_strct *cn)
{
	char       buf[RECV_BUFF_LENGTH];
	struct     stat stbuf;
	char      *file_url;
	int        file_exists;
	time_t     now = time(NULL);
	struct tm *tm_struct;

	/* prepare the global date string */
	if (now-_Last_loop>0) {
		_Last_loop = now;
		tm_struct = gmtime(&_Last_loop);
		/* Sun, 06 Nov 1994 08:49:37 GMT */
		strftime( _Master_date, 30, "%a, %d %b %Y %H:%M:%S %Z", tm_struct);
	}

	file_url = cn->url+1;              /* eat leading slash */
	/* check if we request a static file */
	if (cn->is_static) {
		file_exists = stat(file_url, &stbuf);
		if (file_exists == -1) {
			//send_error(cn, 404);
			printf("Sorry dude, didn't find the file: %s\n", file_url);
			remove_conn_from_list(cn);
			return;
		}

		cn->file_desc = open(file_url, O_RDONLY);
		cn->processed_bytes = (size_t) snprintf(buf, sizeof(buf),
			HTTP_VERSION" 200 OK\r\n"
			"Server: %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %ld\r\n"
			"Date: %s\r\n\r\n",
			//"Last-Modified: %s\r\n",
			_Server_version,
			get_mime_type(file_url),
			(long) stbuf.st_size,
			_Master_date
			//ctime(&stbuf.st_mtime)
		); /* ctime() has a \n on the end */
		send(cn->net_socket, buf, strlen(buf), 0);

		/* FIXME: we assume the head gets send of in one rush */
		cn->req_state = REQSTATE_BUFF_FILE;
	}
	else {
		/* enqueue this connection to the _App_queue */
		pthread_mutex_lock( &pull_job_mutex );
		if (NULL == _Queue_tail) {
			_Queue_tail = _Queue_head = cn;
			_Queue_count = 1;
		}
		else {
			_Queue_tail->q_prev = cn;
			_Queue_tail = cn;
			_Queue_count++;
		}
		pthread_mutex_unlock( &pull_job_mutex );
		cn->req_state  = REQSTATE_BUFF_HEAD;

		/* wake a worker to start the application */
		pthread_cond_signal (&wake_worker_cond);   /* we added one -> we wake one */
	}
}


static void
buff_file (struct cn_strct *cn)
{
	int rv = read(cn->file_desc, cn->data_buf_head, RECV_BUFF_LENGTH);

#if DEBUG_VERBOSE == 1
	printf("\n\nbuffered:%d\n", rv);
#endif

	cn->out_buf = cn->data_buf_head;

	if (0 >= rv) {
		close(cn->file_desc);
		cn->file_desc = -1;
		remove_conn_from_list(cn);
		return;
	}

	cn->processed_bytes = rv;
	cn->req_state = REQSTATE_SEND_FILE;
}

static void
send_file (struct cn_strct *cn)
{
	int rv = send (cn->net_socket, cn->out_buf,
		cn->processed_bytes, 0);

#if DEBUG_VERBOSE == 1
	printf("[%d]sent: %d   ---- left: %d\n",
		cn->id, rv, cn->processed_bytes-rv);
#endif
	if (0 > rv) {
		remove_conn_from_list(cn);
	}
	else if (cn->processed_bytes == rv) {
		if (cn->is_static)
			cn->req_state = REQSTATE_BUFF_FILE;
		else
			remove_conn_from_list(cn);
	}
	else if (0 == rv) {
		/* Do nothing */
	}
	else {
		cn->out_buf = cn->out_buf + rv;
		cn->processed_bytes -= rv;
		//printf("adjusted to %d bytes\n", cn->processed_bytes);
	}
}

static const char
*get_mime_type(const char *name)
{
	/* only bother with a few mime types - let the browser figure the rest out */
	if (strstr(name, ".htm"))
		return "text/html";
	else if (strstr(name, ".css"))
		return "text/css";
	else if (strstr(name, ".js"))
		return "text/javascript";
	else if (strstr(name, ".ico"))
		return "image/vnd.microsoft.icon";
	else
		return "application/octet-stream";
}
