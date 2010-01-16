============================
PARCLE
============================

This starts out as a test of a concept to eventually serve the applications from
lua. The server is modelled after the standard idea of a normal select based
server. Over time it probably won't be converted to more sophisticated concepts
such as kpoll, /dev/poll etc, because we just don't need the power. Since the
main purpose is to serve Lua applications we will most likely run out of steam
before we run out file descriptors. Also please note that it is Linux only at
first. Eventually we might wanna be able to hang connections though. Then select
will be not sufficient anymore. There shall be no major stumbling blocks for
porting it to Mac and even to Windows but the main functionality will be
implemented for Linux first.

Parcle is targeted towards Lua 5.1.4 but will try to move to Lua 5.2 as it comes
out. There shall be no problems when using LuaJIT instead.

Basic ideas it is modelled around:
---------------------------------

 - organize the busy and free connection structs in linked lists for easy
   flexible access
 - instead of working with fixed strings of maximum length, we run on exactly
   one input buffer so we minimize allocations. Crucial information will be
   organized by pointers to null terminated strings in the input buffers. That's
   either a good idea or stupid (because error prone and hard to manage). I
   wanna have at least a shot at it.
 - serve static requests from a fixed length buffer, don't optimize it since it
   is not the primary use case
 - dynamic requests get queued, and then picked of by a thread pool. This does
   not make a great design for comet style since we are limited by the number of
   threads in the pool and can't hang connections. Maybe we can do something
   about that later.
 - responses from dynamic requests get buffered in Lua and the C-part of the
   server gets a pointer to it. This way we can leverage Luas garbage collector.

More information is available from doc/ARCHITECTURE.rst


Compilation:
------------

Check out the future. The main build system is based on clang, the new C
frontend to LLVM. Much better error messages make it all worth wile. Lua must be
installed. Running "make" invokes clang, assuming you have it installed. Running
make with gcc can be done this way:
	make CC=gcc LD=gcc

Plans/Ideas:
------------

 - have Lua included in the build, so that users don't have to rely on
   system-wide installation, which is always a hassle on production environment
 - there shall be a command line tool available, allowing to execute Lua
   functions in the application part as that is very handy for development and
   debugging
 - try to make the communication between the server and the application WSAPI
   compliant if we can get away without further memory allocation
 - utilize Lua_buffer when we come across POST styled requests


Order of processing (none of these steps are guaranteed to succeed in on run,
since it is all non blocking):

	1. read_request()      -> put incoming bytes into the main buffer  
	  - when reached second line interpret the first one and parse url
	  - determine if static or dynamic request
	  - when we hit the end of header go to next step
	2. write_head()        -> write static header to socket
	  - if static, stat(file) and write that info, the proceed to next step
	  - if dynamic, enqueue for Thread pool, remove from select loop
	3. buffer_file()       -> fill buffer with file content or app buffer
	  - if static, fill the buffer and go to next step, this will be repeated
	    until the entire file is sent
	  - if dynamic, wait until the thread has filled the buffer, then send a
	    notice from the thread to the main loop(pipe ipc) that connection x is
	    ready, include that connection's socket in the main loop again
	4. send_file()         -> send buffer to socket
	  - either the file buffer or the application buffer gets sent out




# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
