======================================
Parclate - parcles templating system
======================================


Basic Design:
-------------------------

Parclate is designed to parse XML templates into a Lua nested table
representation. The XML parsing is based on a RegExp parser that was published
by Roberto on the lu-users.org wiki. It is not 100% compliant neither is it DTD
validating but it does find issues in maleformed XML (unclosed tags etc).
Parclates is more partial towards XML than it is to HTML. That means you can use
invalid HTML-tags in the templates. It also means you must close always tags:
<br />, <image src="" />, and <li>item</li> are mandatory. 

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

Okay, this one sucks. Reading implementations, arguments and papers and working
with either kind of templating system left me undecided for a long time. But
like with everything sooner or later I came up with a compromise that works for
me and hopefully for others as well. The overall impression I have is that it
seems to be easier to work with templating systems that do incorporate some sort
of xml/html tags. There is no real technical reason for that in my opinion
because a programmer and even a designer might be able to wrap his or her mind
around any WELL DOCUMENTED implementation. However, the majority of us grew up
with html, php and the like and I have to admit, that neither mako nor haml ever
came natural to me because like anybody else I'm a creature of habit. Moreover,
xml and html being relatives, makes xml a natural choice for an html targeted
templating language. Now, that being said, if you wanna accomplish anything
else other than html, a more generic templating language type such as mako
probably is the better choice. Also I found when working together with designers
on a project it is easier to have an html like templating language as a least
common denominator. Now, all that being said, Parlates core is a Lua table
representation of a DOM. This DOM could as well be generated from another
templating language such as Cheetah, which is rather text based. It is on the
agenda to write something like that. It could be used to generate emails,
dynamically benerated JavaScript.


Internals -> DOM structure -> compiled code
-------------------------------------------

This is mainly a high level schema. tt is used as the parsed template
representation. t is used as the compiled template representation.

Sample template code:
~~~~~~~~~~~~~~~~~~~~~

This is a standard html styled snipped.::

	<ol l:if="show_links_section">
		<!-- A comment, included as string element -->
		<li l:for="i,link in ipairs(links)" style="color:red"
		  l:attrs="{class=(i%2==1) and 'even' or 'odd'}">
			<a l:attrs="{href=link.url}">${link.name}</a> posted by ${link.username}
		</li>
	</ol>

tt:serialize() shall create a semantically equivalent string from a parsed
representation.

DOM structure (note, template whitespace are part f othe chunks):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This representation is catered towards to the XML approach, which why it shall
be able to also represent a more textbased approach quite easily. It really
comes down to the parser. We could utilize a Cheetah like language for that. On
every value the main decision to make is if it is a string (concatenate to
adjacent string) or is it a table (parse the actual "string keys", then the
numeric indexed stuff, repeat recursively). ::

    [1] => '	'
    [2] => {
       [1] => '
    		<!-- A comment, included as string element -->
    		'
       [2] => {
          [1] => '
    			'
          [2] => {
             [cmd] => {
                  [attrs] => '{href=link.url}'
             }
             [empty] => 'false'
             [1] => '${link.name}'
             [tag] => 'a'
          }
          [3] => ' posted by ${link.username}
    		'
          [cmd] => {
             [for] => 'i,link in ipairs(links)'
             [attrs] => '{class=(i%2==1) and 'even' or 'odd'}'
          }
          [empty] => 'false'
          [arg] => {
             [style] => 'color:red'
          }
          [tag] => 'li'
       }
       [cmd] => {
          [if] => 'show_links_section'
       }
       [empty] => 'false'
       [3] => '
    	'
       [tag] => 'ol'
    }

Compiled code (crucial render part only):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is actually enclosed in some protective environment (aka. sandbox). Also
the table x is prefilled with a bunch of empty elements to avoid rehashing
especially in the low numbers (We honour that lua rehashes by 2^x). 'insert',
'format' and co are predefined shortcuts to string.format etc the environment.
Also, the automatically generated code is not quite as pretty formatted as
displayed here of cause. ::

	local x={''}
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
				]])
			insert(x,[[<a]])
			for _at,_atv in pairs({href=link.url}) do
				insert(x, format([=[ %s='%s']=], _at, _atv))
			end
			insert(x, format([[>%s</a>]],link.name))
			insert(x, format([[ posted by %s
			</li>]],link.username))
		end
		insert(x,[[
		</ol>]])
	end
	return concat(x,'')

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

The trailing Whitespace is not honoured. That is a known issue and mostly of
asthetic nature.::

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

Basically, kids attr command, some sort of template inheritance shall be done as
part of the basic functionality. Parclate already includes a "compile to_file"
functionality -> there shall be a convienience wrapper that can allows for easy
bulk compilation and access of compiled templates. Probably directory based.


# vim: ts=4 sw=4 st=4 sta tw=80 ft=rest
