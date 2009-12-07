#!/usr/bin/env lua

-- read a template

package.path = '../?.lua;'..package.path

local Parclate = require('parcle.Parclate')

local template = [[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" 
   "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd">
<html lang="el" xml:lang="en" xmlns="http://www.w3.org/1999/xhtml">
	<head>
		<title>$title</title>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
		<!-- scripts are tougther to parse, so are xml comments -->
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

		<ol l:if="struct.header and show_list">
			<li l:for="title,link in pairs(links)">
				<!-- A comment, shall be ignored -->
				<a href="${link.url}">${title}</a>
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

local value = {
	title   = 'My awesome little website',
	struct  = {header = 'Great stuff from her on down', legal = 'whatever'},
	show_list = true,
	links   = {
		Parcle = {username='Parcle', url='http://parcle.com', time='2009-12-04 15:24'},
		Google = {username='Probiwan Kenobi', url='http://google.ca', time='2009-12-04 13:24'},
		Design = {username='Ursus', url='http://maxdesign.com.au', time='2009-12-04 14:13'},
		Knowledge = {username='Dummy', url='http://ajaxinan.com', time='2009-12-04 11:56'}
	}
}

local t = Parclate(template)  -- gen tmpl representation from xml string
print(t)
print(t:serialize())

