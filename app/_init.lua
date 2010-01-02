Parclate = require('lib.Parclate')
local output={}
local t = Parclate([[<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" >
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en" >
<head>
  <title>A Lua-generated dynamic page</title>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
</head>
<body>
 <span l:for="_=1,90" l:strip="">
  <b>I am a line</b>: Amazing isn't it totally blowing your mind! ?! <br />
 </span>
</body>
</html>]])()

function test (cn)
	--  buffer the entire output
	output[cn] = 'HTTP/1.1 200 OK\r\n' ..
		'Server: testserver/0.1\r\n' ..
		'Content-Type: text/html\r\n' ..
		'Last-Modified: ' .. os.date('%a, %d %b %Y %H:%M:%S') .. '\r\n' ..
		-- 'Content-Length: ' .. #result .. '\r\n' ..
		'\r\n' .. tostring(t)
	parcle.prepare(cn, output[cn])
end

print ('Lua says Initialized')
-- vim: ts=4 sw=4 sts=4 sta tw=80 list
