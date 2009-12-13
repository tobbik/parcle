#!/usr/bin/env lua

-- read a template

package.path = '../?.lua;'..package.path

local Parclate = require('parcle.Parclate')

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
print(t)                      -- output table of tmpl representation
print(t:serialize())          -- output the template minus the command (l:*) tags
print(t:to_file())            -- output the template minus the command (l:*) tags
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
