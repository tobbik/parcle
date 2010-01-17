============================
Parcle
============================

The application framework
-------------------------

The "lib/" directory contains all the modules and classes that make up parcle.
For example routing, request objects or template libraries will be put into this
directory. The code should be restricted to Lua; C libraries shall be located in
the "src/" directory.


Application design
------------------

A parcle application consists of three main directories. For the time being the
names of these dirctories are hardcoded and can't be changed even from a
configuration file. For an example, please check the sample directory that
shipped with the distribution of parcle. The basic layout of an application is
explained based on the example given by the sample application:

   - [dir] webroot -> all static files are served from that directory, that
     includes robots.txt and the favicon.ico (for now, dirty hardcoduing in
     the server)
   - [dir]templates -> all the templates that are used in your application as
     long as they are referenced from a file and not declared inline in your
     application
   - app -> this is the application, structured whatever we end up with ...

# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
