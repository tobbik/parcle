#!/usr/bin/env lua

-- read a template

package.path = '../?.lua;'..package.path

local Parclate = require('lib.Parclate')

local template = [[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" 
   "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd">
<html lang="el" xml:lang="en" xmlns="http://www.w3.org/1999/xhtml">
	<head>
		<title>${title}</title>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
		<!-- scripts are tougher to parse, so are xml comments -->
		<script type="text/javascript" language="JavaScript1.5">
			var long = 123;
			if (long%15 &gt; 7) {
				alert("long is too long!");
			}
		</script>
	</head>
	<body class="index">
		<div id="header">
			<h1>${struct.header}</h1>
		</div>
		<p> Show me the funk: ${func()}</p>

		<ol l:if="struct.header and show_list">
			<li l:for="name,link in pairs(links)" class="link_list">
				<!-- A comment, shall be ignored -->
				<a href="${link.url}">${name}</a> ${title}
				posted by ${link.username} at ${link.time}
			</li>
		</ol>

		<p><a class="action" href="/submit/">Submit new link</a></p>

		<div id="footer">
			<hr />
			<p class="legalese">${struct.legal}</p>
		</div>
	</body>
</html>
]]

local t = Parclate(template)  -- gen tmpl representation from xml string
print(t)                      -- output file representation
-- print(x:serialize())          -- output the template
local x= t:compile()          -- create the source code that serializes the template

x.title     = 'My awesome little website'
x.struct    = { header = 'Great stuff from here on down', legal = 'whatever' }
x.show_list = true
x.links     = {
	Parcle = {username='Parcle', url='http://parcle.com', time='2009-12-04 15:24'},
	Google = {username='Probiwan Kenobi', url='http://google.ca', time='2009-12-04 13:24'},
	Design = {username='Ursus', url='http://maxdesign.com.au', time='2009-12-04 14:13'},
	Knowledge = {username='Dummy', url='http://ajaxinan.com', time='2009-12-04 11:56'}
}
x.func = function() return 'value from func.' end
print(x)
x()


-- nested templates with strip
local ts1 ='<span l:for="k in numbers()" l:strip="">I am the <i>${k}</i> <b>line</b></br />\n</span>'
local a = Parclate(ts1)()  -- gen tmpl representation from xml string
a.numbers   = function ()
	local i=0
	local t={'first','second','third','fourth','fifth','sixth'}
	return function()
		i = i + 1
		if t[i] then return t[i] end
	end
end

print (a)

local ts2 = [[
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" >
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en" >
<head>
  <title>${title}</title>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
</head>
<body>
  ${tostring(nestedloop)}
  <p>TEST: ${char(65,66,67)}</p>
</body>
</html>
]]
local bt = Parclate(ts2)    -- gen tmpl representation from xml string
local b  = Parclate(ts2)()  -- gen tmpl representation from xml string
print(bt)

b.title      = 'An entirely different webtitle'
b.nestedloop = a
b.char       = string.char     -- string char is not known in templates env
print(b)

local docsample = [[
	<ol l:if="show_links_section">
		<li l:for="i,link in ipairs(links)" style="color:red"
		  l:attrs="{class=(i%2==1) and 'even' or 'odd'}">
			<a l:attrs="{href=link.url}">${link.name}</a> posted by ${link.username}
		</li>
	</ol>
]]
local dsr  = Parclate(docsample)    -- gen tmpl representation from xml string
local dst  = dsr()                  -- gen tmpl representation from xml string

print(dsr:debug())
print(dsr:to_file())

dst.show_links_section = true
dst.links              = {
	{name='Parcle',    username='Parclicator',     url='http://parcle.com'},
	{name='Google',    username='Probiwan Kenobi', url='http://google.ca'},
	{name='Design',    username='Cool Stuff',      url='http://maxdesign.com.au'},
	{name='Knowledge', username='Smart Cookie',    url='http://ajaxinan.com'}
}
print(dst)
