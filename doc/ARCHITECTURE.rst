======================
Architecture of parcle
======================

This is a documentation stub, so I won't forget to write what all the code is
about.

High Level:
-------------------------

At the core, parcle is an asynchronous, single threaded and event-driven server
that can easily handle the serving of many static files. The core analyzes each
incoming connection, parses the url to figure out if we want a static file (from
the "webroot" directory) or anything else (which is considered a dynamic
request.) Then dynamic requests are queued for a thread pool. Each thread in the
pool has it's very own Lua interpreter, it's own db connections and is entirely
disconnected from any other Lua states (aka. from any other thing going on in
any other thread). Each thread analyzes the url further, to figure out which
method is to be called, and then calls that method. Lua has access to the data
structure created by the core server, this prevents us from creating hundreds of
copies in memory.

Low Level:
-------------------------

The core of parcle is a very slim httpd server engine. At the moment it utilizes
a select() based loop that allows for the acceptance of connection and the
serving of static files. The server is written in ANSI C and very straight
forward. Some functions in the core allow for high performance parsing of HTTP
headers and payload (that's our term for GET or POST styled encoded data). While
parcle can analyze requested content-types and encoding, it does not use that
for serving standard files. That means if a user requests either .png or .gif we
do not negotiate between them, there is a simple first come first serve scenario
and almost always the request domain url actually specifies the requested files.
Since that is how 99% of the web works anyway, parcle does not do content
negotiation. If you need that proxy your parcle instances through an apache
server that actually can do the negotiation for you. For dynamic requests
however, we are able to parse the headers with the C functions and pass on the
results to the Lua interpreter where a user can utilize that headers for "in
application decisions".  Every request that comes in goes through the following
4 states that are modelled in the loop (none of these steps are guaranteed to
succeed in on run, since it is all non blocking):

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


The connections are organized in four different ways, always using the same kind
of structure that we call cn_strct. There are four different lists, serving
different but related purposes:

   - _Free_conns is a singly linked list, used to store idling cn_strct from
     which we we can pull as soon as a new connection comes in. It is organized
     as a LIFO stack, because we wanna (re)use cn_structs not used so long ago
     in order to make sure to reuse memory soon before it might get paged.
     _Free_conns itself points to the tail of the stack
   - _Busy_conns is a doubly linked list. That saves us a log(n) operation when
     the connection gets removed from the _Busy_conns. _Busy_conns itself
     points to the tail of the stack
   - _Queue_head/_Queue_tail is singly linked list, organized as a FIFO stack,
     used to queue up connections waiting to be processed by the Lua threads.
     _Queue_head organized structs are always part of the _Busy_conns as well,
     because that's  how we determine them as "active".
   - a dynamically sized array we use to index the cn_structs. That is
     necessary to pinpoint cn_structs that are finished in the thread and are
     ready to be included into the io-loop again. Resizing that array happens
     dynamically in steps of the of x^2 to avoid reallocations.


=============================
Utilized or bundled Libraries
=============================

The design of parcle is inspired/motivated by the authors experience with
developing and deploying web-applications. Development and deployment
environments are more often than not differ significantly. While for most
developers it is desirable to keep the development environment up-to-date,
install newer software, try updates etc. it is most desirable to keep
deployment environments stable and rely on long time supported software. If the
application itself depends on many different system-wide installed libraries
and programs, the deployment process easily becomes very cumbersome. The bottom
line is, that the more self-contained a web-application is, the easier it is to
deploy the package and the easier it is to have it installed side by side with
more modern versions of libraries. The serious drawback is, that for some
libraries we have to reinvent the wheel, install it twice or more times onto
the same box. The big advantage is that we might be able to install the
application into one directory that can be tarred up, moved elsewhere etc. This
document is a tad Python centric as ther are quite a few frameworks out there to
compare and to learn from.

What choices are there?
-----------------------

From the authors perspective, when it comes to deployment, web-frameworks can be
divided into two main categories:

	- self contained (ships with all functionalities bundled)
	- leveraging system wide installed libraries

Python's Django-framework has it's own templating system, it's own ORM etc.
whereas Turbogears on the other hand utilizes genshi, formencode, sql-alchemy
etc. With new versions of the utilized libraries coming out, framework users are
at the authors mercy to hope that new versions of the framework incorporate new
versions of libraries with out breaking the applications code base. With the
self-contained frameworks, users must rely on the authors willingness to include
desired functionality into the libs shipped with the framework. There is simply
no silver bullet to provide a smooth upgrade path, especially for applications
with thousands of lines of written code. Inevitably, many users end up
monkey-patching new functionality into older libraries, maintaining the own
versions of libraries and dependent applications, making development harder
because these special libraries will have to be installed on every developers
box. A smooth update path for everyone is the silver bullet and those are hard
to find.

What others do?
---------------

Python's community developed several libraries that enable users to install
complete python environments contained into user-defined directories. Virtualenv
is one of them and it is just easy to setup one directory per application in
order to try new features etc. It gives great flexibility and makes the
development process as well as the deployment process independent from the
system package management. Tools like easy_install or lua rocks always seemed to
be troublesome when being used as root and interfering with files and paths
controlled by the systems package-management be it apt, rpm or pacman.
Containing the installed results into one directory that can be tarred up and
deployed or given to another developer makes the process of getting people up to
speed that much faster and less cumbersome.

For Java applications it is just as easy to utilize the classpath as the
controlling mechanism. Automatized build tools like maven fail to make the
process as transparent as I think it should be and building the application
becomes very network bound. At least the system's installation stays untouched
and whatever becomes installed is contained by the users home directory.

What can be done better?
------------------------

Unfortunately, not much. Every deployment process is defined by individual
requirements and is unique to the situation that is formed by the abilities and
restrictions to the team involved with the development. I came to the conclusion
that utilizing external libraries should be left to the users decision. Give the
users options and make it easy to include libraries. Core functionality shall be
provided out of the box and should not be dependent on other libraries. This
creates overhead in the development process but gives a lot of flexibility which
makes it easier for users to customize the framework. Speaking of customizing.
Monkey-patching isn't nice but sometimes it just can't be avoided. Make it as
simple and as transparent as possible.


# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
