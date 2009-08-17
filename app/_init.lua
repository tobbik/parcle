local result = [[<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" >
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en" >
<head>
  <title>A teestwebsite for static output 10 lines</title>
  <meta http-equiv="content-type" content="text/html; charset=utf-8" />
</head>
<body>
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
  <b>I am a line</b>: Amazing isn\'t it totally blowing your mind! ?! <br />
</body>
</html>]]

-- send the header information    Sun, 06 Nov 1994 08:49:37 GMT
function send_result (sock)
	local header = 'HTTP1.1 200 OK\r\n' ..
		'Server: testserver/0.1\r\n' ..
		'Content-Type: text/html\r\n' ..
		'Last-Modified: ' .. os.date('%a, %d %b %Y %H:%M:%S') .. '\r\n' ..
		'Content-Length: ' .. #result .. '\r\n\r\n' .. result
	while true do
		local last = parcle.send(sock, header, offset)
		print ("LAST:", last)
		if last<#header then
			coroutine.yield(false)
		else
			break
		end
	end
end


print ('Initialized')
-- vim: ts=4 sw=4 softtabstop=4 sta tw=80 list
