#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* A few constants */
#define BACK_LOG                 5

#define MAX_REQUEST_LENGTH       256
#define MAX_READ_LENGTH          8000
#define BLOCK_SIZE               4096
#define RECV_BUFF_LENGTH         8196

#define INITIAL_CONNS            5
#define HTTP_PORT                8000
#define DEBUG_VERBOSE            1

enum req_states
{
	REQSTATE_READ_HEAD,
	REQSTATE_SEND_HEAD,
	REQSTATE_BUFF_FILE,
	REQSTATE_SEND_FILE
};

enum req_types
{
	REQTYPE_GET,
	REQTYPE_HEAD,
	REQTYPE_POST
};

/* contain all metadata regarding one connection */
struct cn_strct
{
	struct  cn_strct   *next;
	enum    req_states  state;
	enum    req_types   reqtype;
	int                 net_socket;

	/* incoming buffer */
	char               *recv_buf;
	char               *recv_buf_head;
	int                 received_bytes;

};

/* global variables */
struct cn_strct     *_Free_conns;
struct cn_strct     *_Busy_conns;
const char * const   _Server_version = "testserver/poc";

/* forward declaration of some connection helpers */
static int  create_listener(int port);
static void handle_new_conn(int listenfd);
static void add_conns_to_list(int sd, char *ip);
static void remove_conn_from_list(struct cn_strct *cn);

/* Forward declaration of some content helpers*/
static void read_request( struct cn_strct *cn );

int
main(int argc, char *argv[])
{
	fd_set              rfds, wfds;
	struct cn_strct    *tp, *to;
	int                 master_sock, rnum, wnum, readsocks;
	int                 i;

	for (i = 0; i < INITIAL_CONNS; i++) {
		tp = _Free_conns;
		_Free_conns = (struct cn_strct *) calloc(1, sizeof(struct cn_strct));
		_Free_conns->recv_buf_head =
			(char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
		_Free_conns->next = tp;
	}

	if ((master_sock = create_listener(HTTP_PORT)) == -1) {
		fprintf(stderr, "ERR: Couldn't bind to port %d\n",
				HTTP_PORT);
		exit(1);
	}

#if DEBUG_VERBOSE == 1
	printf("MASTER SOCKET: %d\n", master_sock);
	printf("%s: listening on port %d (http)\n",
			_Server_version, HTTP_PORT);
#endif

	// main loop
	while (1) {
		// clean socket lists
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		rnum = wnum = -1;

		// Add master listener to reading sockets
		FD_SET(master_sock, &rfds);
		rnum = master_sock;

		// Add the established sockets
		tp = _Busy_conns;

		/* Adding connection to the SocketSets based on state */
		while (tp != NULL) {

			if (tp->state == REQSTATE_READ_HEAD) {
				FD_SET(tp->net_socket, &rfds);
				if (tp->net_socket > rnum)
					rnum = tp->net_socket;
			}

			tp = tp->next;
		}
#if DEBUG_VERBOSE == 1
		printf("b4 START SELECT ---- WNUM: %d    RNUM: %d\n", wnum, rnum);
#endif

		readsocks = select(
			wnum > rnum ? wnum+1 : rnum+1,
			rnum != -1 ? &rfds : NULL,
			wnum != -1 ? &wfds : NULL,
			(fd_set *) 0,
			NULL
		);

		// is the main listener in the read set? -> New connection
		if (FD_ISSET(master_sock, &rfds)) {
			handle_new_conn(master_sock);
			readsocks--;
		}

		// Handle the established sockets
		tp = _Busy_conns;

		while (readsocks > 0 && tp != NULL) {
			to = tp;
			tp = tp->next;

			if (to->state == REQSTATE_READ_HEAD &&
						FD_ISSET(to->net_socket, &rfds)) {
				readsocks--;
				printf("WANNA READ HEAD\n");
				read_request(to);
			}

		}
	}

	return 0;
}

// create the master listening socket
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

	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &tmp_s, sizeof(int));
	if (bind(sd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		close(sd);
		return -1;
	}

	listen(sd, BACK_LOG);
	return sd;
}

/*
 * get a socket and form a cn_strct around it
 *  - either pull it of free_conns or acreate one
 *  - add it to the busy_conns
 * */
static void
add_conn_to_list(int sd, char *ip)
{
	struct cn_strct *tp;

	/* pull out connection struct ... or create one */
	if (NULL == _Free_conns) {
		tp = (struct cn_strct *) calloc (1, sizeof(struct cn_strct));
		tp->recv_buf_head = (char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
	}
	else {
		tp = _Free_conns;
		_Free_conns = tp->next;
		/* TODO: For Later, if we end up reallocing for larger buffers we need
		 * to keep track of how much we need to null over upon reuse
		 */
		memset(tp->recv_buf_head, 0, RECV_BUFF_LENGTH * sizeof(char));
	}

	tp->recv_buf        = tp->recv_buf_head;

	/* Make it part of the busy connection list */
	tp->next = _Busy_conns;
	_Busy_conns = tp;
	tp->net_socket = sd;

	/* Pre/Re-set initial variables */
	tp->state   = REQSTATE_READ_HEAD;
	tp->reqtype = REQTYPE_GET;
	tp->received_bytes  = 0;
}

static void
handle_new_conn(int listen_sd)
{
#if DEBUG_VERBOSE == 1
	printf("starthandling  NEW connection");
#endif
	struct sockaddr_in their_addr;
	socklen_t tp = sizeof(struct sockaddr_in);
	int connfd = accept(listen_sd, (struct sockaddr *)&their_addr, &tp);
	add_conn_to_list(connfd, inet_ntoa(their_addr.sin_addr));
}

void
remove_conn_from_list(struct cn_strct *cn)
{
	struct cn_strct *tp;
	int shouldret = 0;

	tp = _Busy_conns;

	if (tp == NULL || cn == NULL) 
		shouldret = 1;
	else if (tp == cn) 
		_Busy_conns = tp->next;
	else {
		while (tp != NULL) {
			if (tp->next == cn) {
				tp->next = (tp->next)->next;
				shouldret = 0;
				break;
			}

			tp = tp->next;
			shouldret = 1;
		}
	}

	if (shouldret) 
		return;

	/* If we did, add it to the free list */
	cn->next = _Free_conns;
	_Free_conns = cn;

	/* Close it all down */
	if (cn->net_socket != -1) {
		close(cn->net_socket);
	}
}


void
read_request( struct cn_strct *cn )
{
	/* Here is the deal, we read as much as we can into the empty buffer, then
	 * reset the buffer pointer to the end of the read material and append at
	 * next read
	 */
	char *tp, *next;
	int   num_recv;
	printf("%s", "Started to read request\n");

	/* For now assume that RECV_BUFF_LENGTH is enough for one read */
	num_recv = recv(
		cn->net_socket,
		cn->recv_buf,
		RECV_BUFF_LENGTH - cn->received_bytes,
		0
	);

	if (num_recv <= 0) {
		if (num_recv < 0) /* really dead? */
			remove_conn_from_list(cn);
		return;
	}
	printf("We received %d byte of data\n", num_recv);
	printf("%s\n", cn->recv_buf);

	next = tp = cn->recv_buf;

	/* count key elements */
	while (*next != '\0') {
		/* Stop once the head is read */
		if (*next == '\r' || *next == '\n') {
			//cn->filehandle = open(cn->dirname, flags);
			//cn->filehandle = open("./forumbaum.html", flags);
			cn->state = REQSTATE_SEND_HEAD;
			return;
		}

		while (*next != '\r' && *next != '\n' && *next != '\0') 
			next++;

		if (*next == '\r') {
			*next = '\0';
			next += 2;
		}
		else if (*next == '\n')
			*next++ = '\0';

		tp = next;
	}
}

// vim: ts=4 sw=4 softtabstop=4 sta tw=80 list
