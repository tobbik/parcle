#!/usr/bin/env lua

-- read a template

package.path = '../?.lua;'..package.path

local Parclate = require('parcle.Parclate')

local template = [[<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd">
<html att="blah_arg" xml:lang="en" lang="en" >
	<head>
		<title>A Lua-generated dynamic page</title>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
		<!-- scripts are tougther to parse, so are xml comments -->
		<script type="text/javascript" language="JavaScript1.5">
			var long = 123;
			if (long%15 &gt; 7) {
				alert("long is too long!");
			}
		</script>
	</head>
	<body>
		<b>I am <em>the </em><i>first</i> line</b>: Amazing isn't it! <br/>
		<b>I am <em>the</em> <i>second</i> line</b>: Amazing isn't it! <br/>
	</body>
</html>]]


local t = Parclate(template)  -- gen tmpl representation from xml string
print(t)
print(t:serialize())

