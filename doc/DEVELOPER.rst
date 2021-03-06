============================
Developer information
============================

Guidelines for the parcle development
-------------------------------------

Parcle is written in Lua, a small, lightweight and very flexible scripting
language. The majority of web-frameworks these days are written utilizing the
MVC design pattern. MVC is easier to implement when the framework is following
an object oriented programming(referred to as OOP from here on) style and the
majority of functionalities is provided as classes.

Lua, by default, is not an object oriented language. However, it's flexibility
allows for the implementation of our own class/oop design. This document will
describe the applied OOP variation choosen for parcle. All classes used for
parcle shall follow these guidelines.

The rules in this document apply to parcle itself. Users can and are encouraged
to apply their own design to their applications as the rules in this document
mainly reflect parcles authors opinion on coding style. If you intend to
contribute to parcle's core itself, you should use the coding guidelines in this
document as it makes it more likely for patches to be accepted towards upstream
development.

Coding rules
------------
Parcle has a C core (server and some libraries). For these C files we use the
following convention, widely known as "kernel style"

   - function declaration span 3 lines: 1st return type, 2nd function name and
     arguments and 3rd opening curly bracket aligned to the left
   - loops and conditions statements opening curly brackets are on the same
     line
   - else statements open a new line, aligned with the preceding if statement

Tabs/spaces: Parcles code, Lua or C uses tabs, default to 4 chars wide

Lua class design
----------------

There are many suggested implementations for OOP characteristics in Lua. Luas
design by default enables to design a more prototype based inheritance and
instantiation rather than a class->instance based design as it is widely known
in the Java/C#/C++ world. For the sake of the omitted (not missing) "new"
keyword in Lua, the "blue PiL" [#]_ suggest a syntax like this to instantiate new
objects: instance = Classname:new() , where new() is a method on a table, by
convention the constructor of the class. However, looking at other languages,
especially python, instance = Classname() might be a cleaner/nicer way to call
the constructor. Lua's meta-tables and the associated __call operator offers
an excellent path to an implementation that makes this sort of syntactic sugar
easy to achieve.

Parcle's classes are NOT implemented using the module function. After reading
the mailing list and the excellent "critique of the module function"
modcrtique_ available on the lua-users wiki, it seems most desireable to
provide good encapsulation and allow to be flexible with names upon import, aka.
local thisName=require('Classname')


   .. [#] Roberto Ierusalimschy, "Programming in Lua", 1st edition, 2003, _OOP in
      Lua: http://www.lua.org/pil/16.html
   .. _modcrtique: http://lua-users.org/wiki/LuaModuleFunctionCritiqued 


# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
