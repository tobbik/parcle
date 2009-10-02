#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* network, sockets, accept, IP handling */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <time.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* A few constants */
#define BACK_LOG                 5

#define MAX_REQUEST_LENGTH       256
#define MAX_READ_LENGTH          94
#define BLOCK_SIZE               4096
#define RECV_BUFF_LENGTH         8196

#define INITIAL_CONNS            5
#define HTTP_PORT                8000
#define HTTP_VERSION             "HTTP1.1"
#define DEBUG_VERBOSE            0
#define WEB_ROOT                 "./"
#define STATIC_ROOT              "webroot"
#define STATIC_ROOT_LENGTH       7

enum bool
{
	false,
	true
};

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

enum http_version
{
	HTTP_09,
	HTTP_10,
	HTTP_11
};

/* contain all metadata regarding one connection */
struct cn_strct
{
	struct  cn_strct     *next;
	enum    req_states    req_state;
	int                   net_socket;
	int                   file_desc;

	/* incoming buffer */
	char                 *data_buf_head;      /* points to start, always */
	char                 *data_buf;           /* points to current spot */
	int                   processed_bytes;
	/* inc buffer state */
	int                   line_count;
	/* head information */
	enum    req_types     req_type;
	char                 *url;
	char                 *pay_load;            /* either GET or POST data */
	enum    http_version  http_prot;

	/* Lua state -> a "lua thread" aka. coroutine */
	enum    bool          is_static;
	lua_State            *Lua;
};

// test lua execution
lua_State            *_L;


/* global variables */
struct cn_strct     *_Free_conns;       /* idleing conns */
struct cn_strct     *_Busy_conns;       /* conns bound to actions */
const char * const   _Server_version = "testserver/poc";
int                  _Master_sock;      /* listening master socket */

/* forward declaration of some connection helpers */
static int   create_listener        ( int port );
static void  handle_new_conn        ( int listenfd );
static void  add_conn_to_list       ( int sd, char *ip );
static void  remove_conn_from_list  ( struct cn_strct *cn );

/* Forward declaration of select's processing helpers */
static void  read_request           ( struct cn_strct *cn );
static void  write_head             ( struct cn_strct *cn );
static void  buff_file              ( struct cn_strct *cn );
static void  send_file              ( struct cn_strct *cn );

/* Forward declaration of string parsing methods */
static void  parse_first_line       ( struct cn_strct *cn );
static const char  *getmimetype     ( const char *name );

/* Forwad declaration of lua bound methods */
static int   l_send_chunk           ( lua_State *L );

/* set up the Lua bindings for C-functions */
static const struct luaL_reg app_lib [] = {
	{"send",   l_send_chunk},
	{NULL,     NULL}
};



/* clean up after ourselves */
static void
clean_on_quit(int sig)
{
	struct cn_strct *tp;

	while (NULL != _Free_conns) {
		tp = _Free_conns->next;
		free(_Free_conns->data_buf_head);
		free(_Free_conns);
		_Free_conns = tp;
	}

	while (NULL != _Busy_conns) {
		tp = _Busy_conns->next;
		free(_Busy_conns->data_buf_head);
		close(_Busy_conns->net_socket);
		free(_Busy_conns);
		_Busy_conns = tp;
	}
	close(_Master_sock);
	_Master_sock = -1;
	printf("Graceful exit done after signal: %d\n", sig);

	/* cleanup Lua */
	lua_close(_L);

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
	fd_set              rfds, wfds;
	struct cn_strct    *tp, *to;
	int                 rnum, wnum, readsocks;
	int                 i;

	signal(SIGQUIT, die);
	signal(SIGTERM, die);
	signal(SIGINT, clean_on_quit);

	// DIRTY!!! we work out of the webroot directory -> change to it
	if (chdir(WEB_ROOT))
		clean_on_quit(2);

	/* initialize Lua and load base libs for the app engine */
	_L = lua_open();
	luaL_openlibs(_L);
	luaL_openlib(_L, "parcle", app_lib, 0);
	i = luaL_dofile(_L, "app/_init.lua");

	/* Fill up the initial connection lists */
	for (i = 0; i < INITIAL_CONNS; i++) {
		tp = _Free_conns;
		_Free_conns = (struct cn_strct *) calloc(1, sizeof(struct cn_strct));
		_Free_conns->data_buf_head =
			(char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
		_Free_conns->Lua = lua_newthread(_L);
		_Free_conns->next = tp;
	}

	/* create the master listener */
	if ((_Master_sock = create_listener(HTTP_PORT)) == -1) {
		fprintf(stderr, "ERR: Couldn't bind to port %d\n",
				HTTP_PORT);
		exit(1);
	}

#if DEBUG_VERBOSE == 1
	printf("%s: listening on port %d (http)\n",
			_Server_version, HTTP_PORT);
#endif

	/* main loop */
	while (1) {
		// clean socket lists
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		rnum = wnum = -1;

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
			tp = tp->next;
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
			tp = tp->next;

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
	}
	return 0;
}


/*____ ___  _   _ _   _   _   _ _____ _     ____  _____ ____  ____
 / ___/ _ \| \ | | \ | | | | | | ____| |   |  _ \| ____|  _ \/ ___|
| |  | | | |  \| |  \| | | |_| |  _| | |   | |_) |  _| | |_) \___ \
| |__| |_| | |\  | |\  | |  _  | |___| |___|  __/| |___|  _ < ___) |
 \____\___/|_| \_|_| \_| |_| |_|_____|_____|_|   |_____|_| \_\____/ */
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

/*
 * get a socket and form a cn_strct around it
 *  - either pull it of free_conns or acreate one
 *  - add it to the busy_conns
 * */
static void
add_conn_to_list(int sd, char *ip)
{
	struct cn_strct *tp;

	/* pop a cn_strct from the free list ... or create one */
	if (NULL == _Free_conns) {
		tp = (struct cn_strct *) calloc (1, sizeof(struct cn_strct));
		tp->data_buf_head = (char *) calloc (RECV_BUFF_LENGTH, sizeof (char));
		tp->Lua = lua_newthread(_L);
	}
	else {
		tp = _Free_conns;
		_Free_conns = tp->next;
		/* TODO: For Later, if we end up reallocing for larger buffers we need
		 * to keep track of how much we need to null over upon reuse
		 */
		memset(tp->data_buf_head, 0, RECV_BUFF_LENGTH * sizeof(char));
	}

	tp->data_buf        = tp->data_buf_head;

	/* Make it part of the busy connection list */
	tp->next = _Busy_conns;
	_Busy_conns = tp;
	tp->net_socket = sd;

	/* Pre/Re-set initial variables */
	tp->req_state = REQSTATE_READ_HEAD;
	tp->req_type  = REQTYPE_GET;
	tp->processed_bytes  = 0;
	tp->line_count       = 0;
	tp->pay_load         = '\0';
	tp->is_static        = false;
}

static void
handle_new_conn(int listen_sd)
{
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


/*___  _____ _     _____ ____ _____   ____  ____   ___   ____
/ ___|| ____| |   | ____/ ___|_   _| |  _ \|  _ \ / _ \ / ___|
\___ \|  _| | |   |  _|| |     | |   | |_) | |_) | | | | |
 ___) | |___| |___| |__| |___  | |   |  __/|  _ <| |_| | |___
|____/|_____|_____|_____\____| |_|   |_|   |_| \_\\___/ \____| */
/* Here is the deal, we read as much as we can into the empty buffer, then
 * reset the buffer pointer to the end of the read material and append at
 * next read
 */
void
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

	// sanity check
	if (num_recv <= 0) {
		if (num_recv < 0) /* really dead? */
			remove_conn_from_list(cn);
		return;
	}

	// set the read pointer to where we left off
	next = cn->data_buf_head + cn->processed_bytes;

	// adjust buffer
	cn->processed_bytes += num_recv;
	cn->data_buf = cn->data_buf_head + cn->processed_bytes;

	// null terminate the current buffer
	cn->data_buf_head[cn->processed_bytes] = '\0';

	/* a naive little line parser */
	while ( (*next != '\0') ) {
		switch (*next) {
			case '\r':
				if (*(next+1)=='\n' ) {
					cn->line_count++;
					if (1 == cn->line_count) {
						parse_first_line(cn);
					}
					if (*(next+2)=='\r' && *(next+3)=='\n'  ) {
						// proceed next stage
						cn->req_state = REQSTATE_SEND_HEAD;
					}
				}
				break;
			default:
				break;
		}
		next++;
	}
#if DEBUG_VERBOSE == 1
	if (REQSTATE_SEND_HEAD == cn->req_state) {
		printf("METHOD: %d\n",   cn->req_type);
		printf("URL: %s\n",      cn->url);
		printf("PROTOCOL: %d\n", cn->http_prot);
		printf("PAYLOAD: %s\n",  cn->pay_load);
	}
#endif
}

/*
 */
void
write_head (struct cn_strct *cn)
{
	char buf[RECV_BUFF_LENGTH];
	struct stat stbuf;
	time_t now = time(NULL);
	char date[32];
	int file_exists;

	strcpy(date, ctime(&now));

	/* check if we request a static file */
	if (cn->is_static) {
		cn->url++;              /* eat leading slash */
		printf("YES, that will be static!\n");
		file_exists = stat(cn->url, &stbuf);

		if (file_exists == -1)
		{
			//send_error(cn, 404);
			printf("Sorry dude, didn't find the file: %s\n", cn->url);
			remove_conn_from_list(cn);
			return;
		}

		cn->file_desc = open(cn->url, O_RDONLY);

		snprintf(buf, sizeof(buf),
			HTTP_VERSION" 200 OK\nServer: %s\n"
			"Content-Type: %s\nContent-Length: %ld\n"
			"Date: %sLast-Modified: %s\n", _Server_version,
			getmimetype(cn->url), (long) stbuf.st_size,
			date, ctime(&stbuf.st_mtime)
		); /* ctime() has a \n on the end */
		send(cn->net_socket, buf, strlen(buf), 0);

		/* FIXME: we assume the head gets send of in one rush */
		cn->req_state = REQSTATE_BUFF_FILE;
	}
	else {
		// execute application
		lua_getglobal(cn->Lua, "send_result");        /* function to be called */
		lua_pushnumber(cn->Lua, cn->net_socket);      /* push socket */
		lua_resume(cn->Lua, 1);                       /* do thread, 1 arg */
		lua_pop(cn->Lua, 1);                          /* clean the stack */

		/* FIXME: we assume the head gets send of in one rush */
		cn->req_state = REQSTATE_SEND_FILE;
	}
}


void
buff_file (struct cn_strct *cn)
{
	int rv = read(cn->file_desc, cn->data_buf_head, RECV_BUFF_LENGTH);
#if DEBUG_VERBOSE == 1
	printf("\n\nbuffered:%d\n", rv);
#endif
	cn->data_buf    =    cn->data_buf_head;

	if (rv <= 0) {
		close(cn->file_desc);
		cn->file_desc = -1;
		remove_conn_from_list(cn);
		return;
	}

	cn->processed_bytes = rv;
	cn->req_state = REQSTATE_SEND_FILE;
}

void
send_file (struct cn_strct *cn)
{
	int rv;
	if (cn->is_static) {
		rv = send (cn->net_socket, cn->data_buf,
			cn->processed_bytes, 0);

#if DEBUG_VERBOSE == 1
		printf("sent:%d   ---- left: %d\n", rv, cn->processed_bytes);
#endif
		if (rv < 0) {
			remove_conn_from_list(cn);
		}
		else if (rv == cn->processed_bytes) {
			cn->req_state = REQSTATE_BUFF_FILE;
		}
		else if (0 == rv) {
			/* Do nothing */
		}
		else {
			cn->data_buf = cn->data_buf + rv;
			cn->processed_bytes -= rv;
		}
	}
	else {
		lua_resume(cn->Lua, 1);                   /* do thread, 1 arg */
		cn->req_state = REQSTATE_BUFF_FILE;
	}
}

/*___   _    ____  ____  _____   _   _ _____ _     ____  _____ ____  ____
|  _ \ / \  |  _ \/ ___|| ____| | | | | ____| |   |  _ \| ____|  _ \/ ___|
| |_) / _ \ | |_) \___ \|  _|   | |_| |  _| | |   | |_) |  _| | |_) \___ \
|  __/ ___ \|  _ < ___) | |___  |  _  | |___| |___|  __/| |___|  _ < ___) |
|_| /_/   \_\_| \_\____/|_____| |_| |_|_____|_____|_|   |_____|_| \_\____/ */

/*
 * * Isolate "METHOD URL?GET_PARAMS HTTP_VER" from first request line
 * - count '/' to help the url delimiter, count '?/ to get parser
 */
void
parse_first_line( struct cn_strct *cn )
{
	char          *next  = cn->data_buf_head;
	unsigned short got_get=0, get_cnt=0, slash_cnt=0, error=0;
	/* METHOD */
	if (0 == strncasecmp(next, "GET",  3)) { cn->req_type=REQTYPE_GET;  next+=3;}
	if (0 == strncasecmp(next, "HEAD", 4)) { cn->req_type=REQTYPE_HEAD; next+=4;}
	if (0 == strncasecmp(next, "POST", 4)) { cn->req_type=REQTYPE_POST; next+=4;}
	*next = '\0';
	/* URL */
	next++;
	if ('/' == *next) {
		cn->url = next;
		if (0 == strncasecmp(next+1, STATIC_ROOT, STATIC_ROOT_LENGTH)) {
			cn->is_static = true;
		}
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
	/* GET - count get parameters */
	if (0 == strncasecmp(next, "HTTP/0.9", 8)) { cn->http_prot=HTTP_09; }
	if (0 == strncasecmp(next, "HTTP/1.0", 8)) { cn->http_prot=HTTP_10; }
	if (0 == strncasecmp(next, "HTTP/1.1", 8)) { cn->http_prot=HTTP_11; }
#if DEBUG_VERBOSE==1
	printf("URL SLASHES: %d -- GET PARAMTERS: %d --ERRORS: %d --LUA: %d\n",
		slash_cnt, get_cnt, error, cn->is_static);
#endif
}

static const char
*getmimetype(const char *name)
{
	/* only bother with a few mime types - let the browser figure the rest out */
	if (strstr(name, ".htm"))
		return "text/html";
	else if (strstr(name, ".css"))
		return "text/css";
	else if (strstr(name, ".js"))
		return "text/javascript";
	else
		return "application/octet-stream";
}

/*_                  _ _ _
 | |   _   _  __ _  | (_) |__  ___
 | |  | | | |/ _` | | | | '_ \/ __|
 | |__| |_| | (_| | | | | |_) \__ \
 |_____\__,_|\__,_| |_|_|_.__/|___/ */

/*
 * FIXME: less than ideal, we tonumber the socket into Lua, we shall use the
 * cn_strct as lightuserdata instead
 * @param:        the socket reference
 * @param:        the string reference
 * @param:        current sending offset in the string
 * @return:       string offset after sending
 */
static int
l_send_chunk (lua_State *L)
{
	int         sockfd;
	const char *data     = NULL;
	int         length   = 0;
	int         offset   = 0 ;

	sockfd =  lua_tonumber(L, 1);
	data   =  lua_tostring(L, 2);
	length =  lua_strlen  (L, 2 );

	offset += write( sockfd, data + offset, length - offset );
	printf("EXECUTE SENDING\n");
	lua_pushnumber(L, offset);  /* how much did we get done? */

	return 1;
}

// vim: ts=4 sw=4 sts=4 sta tw=80 list
