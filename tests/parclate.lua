#!/usr/bin/env lua

-- read a template

package.path = '../?.lua;'..package.path

local Parclate = require('parcle.Parclate')

local template = [[



<html att="blah_arg" xml:lang="en" lang="en" >
	<head>
		<title>A Lua-generated dynamic page</title>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
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

