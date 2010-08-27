======================================
Parclate - parcles templating system
======================================


Basic Design:
-------------------------

Parclate is designed to parse XML templates into a Lua nested table
representation. The XML parsing is based on a RegExp parser that was published
by Roberto on the lu-users.org wiki. It is not 100% compliant neither is it DTD
validating but it does find issues in malformed XML (unclosed tags etc).
Parclates is more partial towards XML than it is to HTML. That means you can use
invalid HTML-tags such as <blah> </blah> in the templates. It also means you
must close always tags: <br />, <image src="" />, and <li>item</li> are
mandatory.

That being said, Parclate will NOT do the formatting for you. Parclates parser
instead tries to preserve the formatting in the template to the best of it's
abilities. There is one notably exception here: long tags with line breaks in
the template will be serialized onto one long line. Also the order of serialized
attributes IS arbitrary. There is no rule in HTML that would enforce their order
anyway.

Serialization in parclate is implemented as a concatenating lua function. When a
template gets serialized, Parclate creates string constants that are as long as
possible. This flow will only be interrupted by variable substitution or other
template functionality such as conditions or loops. The result of parclates
rendering is a string buffer.


XML vs non-XML vs HTML(5)
-------------------------

Well, this one sucks. Reading implementations, arguments and papers and working
with either kind of template systems left me undecided for a long time. But like
with everything sooner or later there was a compromise that worked for me and
hopefully for others as well. The overall impression I have is that it seems to
be easier to work with template systems that do incorporate some sort of
xml/html tags. There is no real technical reason for that in my opinion because
a programmer and even a designer might be able to wrap his or her mind around
any WELL DOCUMENTED implementation. However, the majority of us grew up with
html, php and the like and I have to admit, that neither mako nor haml ever came
natural to me because like anybody else I'm a creature of habit. Moreover, xml
and html being relatives, makes xml a natural choice for an html targeted
template language. Now, that being said, if you want to accomplish anything else
other than html, a more generic template language type such as mako is probably
the better choice. Yet, when working together with designers on a project I
found it to be easier to have an html like template language as a least common
denominator. So it comes down to the scenario you find yourself in. Now, all
that being said, Parlates core is a Lua table representation of a DOM.  This DOM
could be as well generated from another template language such as Cheetah, which
is rather text based. It is on my agenda to write something like that. It could
be used to generate emails, dynamically generated JavaScript etc.


Internals
---------

Template -> Lua table -> compiled code:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Template code gets firstly parsed and then compiled.  This is mainly a high
level overview on the stages a template passes through. 'tt' is used as the
parsed template representation. 't' is used as the compiled template
representation.

Sample template code:
~~~~~~~~~~~~~~~~~~~~~

This is a standard html styled snippet. ::

		<ol l:if='show_links_section'>
			<!-- A comment, included as string element -->
			<li l:for='i,link in ipairs(links)' style='color:red'
			  l:attrs='{class=(i%2==1) and 'even' or 'odd'}'>
				<a l:attrs='{href=link.url}'>${link.name}</a> posted by ${link.username}
			</li>
		</ol>

tt:serialize() shall create a semantically equivalent string from a parsed
representation.

DOM structure (note, template whitespace are part of the chunks):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This representation is catered towards to the XML approach, which why it shall
be able to also represent a more text-based approach quite easily. It really
comes down to the parser. We could utilize a Cheetah like language for that. On
every value the main decision to make is if it is a string (concatenate to
adjacent string) or is it a table (parse the actual "string keys", then the
numeric indexed stuff, repeat recursively). ::

	[1] => '	'
	[2] => {
	   [cmd] => {
	        [if] => 'show_links_section'
	   }
	   [empty] => 'false'
	   [tag] => 'ol'
	   [1] => '
			<!-- A comment, included as string element -->
			'
	   [2] => {
	        [arg] => {
	             [style] => 'color:red'
	        }
	        [attrs] => '{class=(i%2==1) and 'even' or 'odd'}'
	        [cmd] => {
	             [for] => 'i,link in ipairs(links)'
	        }
	        [empty] => 'false'
	        [tag] => 'li'
	        [1] => '
				'
	        [2] => {
	             [attrs] => '{href=link.url}'
	             [empty] => 'false'
	             [tag] => 'a'
	             [1] => '${link.name}'
	        }
	        [3] => ' posted by ${link.username}
			'
	   }
	   [3] => '
		'
	}

Compiled code (crucial render part only):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is actually enclosed in some protective environment (aka. sandbox).
Depending on the Lua version either setfenv or _ENV is used. Also the table x is
pre-filled with a bunch of empty elements to avoid rehashing especially in the
low numbers (We honour that Lua rehashes by 2^x). 'insert', 'format' and co are
predefined shortcuts to string.format etc the environment.  Also, the
automatically generated code is not quite as prettily formatted as displayed here
of cause. ::

	local x={'','','','','','','','','','','','','','','','',''}
	insert(x,[[	]])
	if show_links_section then
		insert(x,[[<ol>
		<!-- A comment, included as string element -->
		]])
		for i,link in ipairs(links) do
			insert(x,[[<li style='color:red']])
			for _at,_atv in pairs({class=(i%2==1) and 'even' or 'odd'}) do
				insert(x, format([=[ %s='%s']=], _at, _atv))
			end
			insert(x,[[>
			<a]])
			for _at,_atv in pairs({href=link.url}) do
				insert(x, format([=[ %s='%s']=], _at, _atv))
			end
			insert(x, format([[>%s</a> posted by %s
		</li>]],link.name,link.username))
		end
		insert(x,[[
	</ol>]])
	end
	return concat(x,'') end

Sample data applied:
~~~~~~~~~~~~~~~~~~~~

This technically effects the environment the render function is executed in.
This way we can slip global or local functions into the environment if really
needed. The environment already knows the following:::

	env = {
		format = string.format, pairs = pairs, ipairs = ipairs,
		concat = table.concat,  insert = table.insert, tostring = tostring
	}

And here we fill our values in:::

	t.show_links_section = true
	t.links              = {
		Parcle    = {username='Parclicator',     url='http://parcle.com'},
		Google    = {username='Probiwan Kenobi', url='http://google.ca'},
		Design    = {username='Cool Stuff',      url='http://maxdesign.com.au'},
		Knowledge = {username='Smart Cookie',    url='http://ajaxinan.com'}
	}

Generated output as by tostring(t):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The trailing white space is not honoured. That is a known issue and mostly of
aesthetic nature.::

		<ol>
			<!-- A comment, included as string element -->
			<li style='color:red' class='even'>
				<a href='http://parcle.com'>Parcle</a> posted by Parclicator
			</li><li style='color:red' class='odd'>
				<a href='http://google.ca'>Google</a> posted by Probiwan Kenobi
			</li><li style='color:red' class='even'>
				<a href='http://maxdesign.com.au'>Design</a> posted by Cool Stuff
			</li><li style='color:red' class='odd'>
				<a href='http://ajaxinan.com'>Knowledge</a> posted by Smart Cookie
			</li>	</ol>

Future Ideas
------------

Basically, define internally used variables (prefix by an _underdash) and
protecting them on _newindex as well a _index and some sort of template
inheritance shall be done as part of the basic functionality. Parclate already
includes a "compile to_file" functionality -> there shall be a convenience
wrapper that can allows for easy bulk compilation and access of compiled
templates. Probably directory based, which will need some native code since Lua
has no directory iterator.

Usage
-----

Invoking Parclate is a two step process. The first step is always the parsing.
Parclate is like a class. Very much like in Python, consider calling it to be
it's constructor. Here are the major steps::

	-- construct the compiled template however you like
	t  = Parclate(t_string)      -- creates a parsed template object
	tc = t:compile()             -- tc is a compiled template
	tc = t()                     -- same -> convenience shortcut
	tc = Parclate(t_string)()    -- can be done in one step
	
	-- fill the variables in the template
	tc.a=1                       -- some example variables
	tc.b={k='name',x='place'}
	print(tc)                    -- print() runs the tostring() method
	tc()                         -- executing it sets all variables to nil
	print(tc)                    -- FAIL! Because nil can't be concatenated

A parsed template like 'tc' in the example above has the following methods which
can be used for debugging and other things::

	t=Parclate(t_string) -- creates a parsed template object
	t:serialize()        -- returns the to XML, semantically identical to
	                     -- 't_string'
	t:debug()            -- pretty prints the internal representation
	t:to_file()          -- returns a string that contains lua code which can
	                     -- be saved to file and require() from there, contains
	                     -- compiled template

A compiled template has only very few commands on it. Basically just the __call
and the __tostring metamethods are hooked up::

	tc=Parclate(t_string)() -- creates a compiled template object
	tc.x=y                  -- assign variable ${x}
	tostring(tc)            -- returns the rendered template as string
	tc()                    -- flushes all variables like tc.x==nil

Schematic use cases in web frameworks and CGI
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When using Parclate it is important to initialize, parse and compile the
templates on startup time of the application so it happens only once. The
compiled template will be used over and over again. That is relatively simple.
Assume an application like most application for WSAPI or Orbit. The following
works on a high level and must be tweaked to apply to actual frameworks.::

	-- self explanatory, make sure the path finds it
	Parclate = require('Parclate')

	-- Parclate(x) parses it, calling() it runs the compile function
	local t1 = Parclate(string_containing_template)()

	-- that gets called when the client hits a routed url
	local return_index_page =  function(web_object)
	    -- do things with web_object, call db etc, define variables
	
	    -- now fill the template, depending on template structure, the variables
	    -- can be literals, tables or even functions
	    t1.var1=x
	    t1.var2=y
	    t1.var3=z
	
	    -- tostring(t1) renders the template into a string
	    return tostring(t1)
	end

The above code assumes that the code is loaded for the runtime of the server
(aka. NOT like a CGI script). Then the compiled template stays in memory and can
be run over and over again. Compiled templates are optimized for speed and
rather low memory consumption. If you need to use CGI scripts that get loaded
over and over again, Parclate has the to_file option, which is yet not
completely usable in a convenient way. Consider the following code, which is
NOT part of your CGI script::

	Parclate = require('Parclate')

	-- Parclate(x) parses it, DO NOT COMPILE IT
	local t2 = Parclate(string_containing_template)

	-- fc will contain a string that when saved in a file provides the same
	-- functionality as a compiled
	local f=io.open('c_template.lua','w')
	f:write( t2.to_file() )
	f:close()

This will leave a compiled Parclate template on your hard drive, which is simply
a .lua file that can be loaded by require(). That can now be used in a CGI
script since it is easily and quickly loaded::

	-- just require the compiled template and you are done
	local t1 = require('c_template')
	
	-- that gets called when the client hits a routed url
	local run =  function(web_object)
	    -- do things with web_object, call db etc, define variables
	
	    -- now fill the template, depending on template structure, the variables
	    -- can be literals, tables or even functions
	    t1.var1=x
	    t1.var2=y
	    t1.var3=z
	
	    -- tostring(t1) renders the template into a string
	    return tostring(t1)
	end


# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
