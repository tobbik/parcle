-- import Lua built in dependencies
local string = require( 'string' )
local table  = require( 'table'  )

local pairs        = pairs
local setmetatable = setmetatable
local tostring     = tostring
local type         = type
local unpack       = unpack

-- implementation
local Parclate = {}

-- THE PARSING
-- helper to disect arguments of tags
local parse_args = function ( s, tag, empty )
	local args     = {}
	local cmds     = {}
	local haveargs = false
	local havecmds = false
	string.gsub(s, '([^%s="]+)=(["\'])(.-)%2', function (w, _, a)
		local ns,cmd = string.match(w,'^(l):(%w+)')
		if ns and cmd then
			havecmds  = true
			cmds[cmd] = a
		else
			haveargs  = true
			args[w]   = a
		end
	end)
	if havecmds and haveargs then
		return {tag=tag, arg=args, cmd=cmds, empty=empty}
	elseif havecmds then
		return {tag=tag, cmd=cmds, empty=empty}
	elseif haveargs then
		return {tag=tag, arg=args, empty=empty}
	else
		return {tag=tag, empty=empty}
	end
end

-- Do the actual parsing aka. xml->table conversion
local parse = function ( s )
	local stack = {}
	local top   = {}
	table.insert(stack, top)
	local ni,c,label,xarg,empty
	local i, j = 1, 1
	while true do
		ni,j,c,label,xarg,empty = string.find(s, '<(%/?)([%w:]+)(.-)(%/?)>', i)
		if not ni then break end
		if i ~= ni then  -- if text is longer than 0 chars
			table.insert(top, string.sub(s, i, ni-1)) -- insert text chunk
		end
		if empty == '/' then    -- empty element tag
			if '' ~= xarg then
				table.insert(top, parse_args(xarg, label, true))
			else
				table.insert(top, {tag=label, empty=true})
			end
		elseif c == '' then     -- start tag
			if '' ~= xarg then
				top = parse_args(xarg, label, false)
			else
				top = {tag=label, empty=false}
			end
			table.insert(stack, top)   -- new level
		else                    -- end tag
			local toclose = table.remove(stack)  -- remove top
			top = stack[#stack]
			if #stack < 1 then
				error('nothing to close with <'..label..'>')
			end
			if toclose.tag ~= label then
				error('trying to close <'..toclose.tag..'> with <'..label..'>')
			end
			table.insert(top, toclose)
		end
		i = j+1
	end
	if #stack > 1 then
		error('unclosed <'..stack[#stack].tag..'>')
	end
	return stack[1]
end

-- THE SERIALIZING
-- serializes one table into (x)html template-less syntax (no l:* commands)
local serialize = function (self)
	local cl = {}
	local s_tag -- pre-define as local -> called recursively
	s_tag = function (t)
		if t.tag then
			table.insert(cl, string.format('<%s', t.tag))
			if t.arg then
				for k,v in pairs(t.arg) do
					table.insert(cl, string.format(' %s="%s"', k, v))
				end
			end
			table.insert(cl, t.empty and ' />' or '>')
			if t.empty then return end
		end
		for k,v in ipairs(t) do
			if 'table' == type(v) then
				s_tag(v)
			elseif 'string' == type(v) then
				table.insert(cl, v)
			else
				error('There is an error in the representation of the template')
			end
		end
		-- close tag
		if t.tag then
			table.insert(cl, string.format('</%s>', t.tag))
		end
		return
	end           -- recursive lexical s_tag
	s_tag(self)
	return table.concat(cl,'')
end
Parclate.serialize = serialize

-- THE COMPILER
-- #private: create lua function source code from the arguments
local dispatch_command = function(cmd, tmpl)
	for c,exp in pairs(cmd) do
		if 'if' == c then
			table.insert(tmpl, string.format('\tif %s then\n\t\t', exp) )
		end
		if 'for' == c then
			table.insert(tmpl, string.format('\tfor %s do\n\t\t', exp) )
		end
	end
end

-- #private: helper to close open functions in the compiled chunk
local dispatch_command_end = function(cmd, tmpl)
	for c,_ in pairs(cmd) do
		if 'if' == c or 'for' == c then
			table.insert(tmpl, '\tend\n')
		end
	end
end

-- #private: combine the last set of continous strings into one chunk
local compile_chunk = function(tmpl, buffer, f_args)
	if 0 == #f_args then
		table.insert(tmpl,
			'\tinsert(x,[[' .. table.concat(buffer,'') .. ']])\n'
		)
	else
		table.insert(tmpl,
			'\tinsert(x, format([[' ..
				table.concat(buffer,'') ..
			']],'.. table.concat(f_args, ',') ..'))\n'
		)
		f_args=nil
	end
	buffer=nil
end

-- #public:
local compile = function (self)
	local tmpl   = {'local t={}\n\nlocal r = function(self)\n\tlocal x={}\n'}
	local buffer = {}
	local f_args = {}
	local c_tag         -- predeclare local for recursive calls
	c_tag = function(t)
		if t.cmd then
			compile_chunk(tmpl, buffer, f_args)
			f_args={}
			buffer={}
			dispatch_command(t.cmd, tmpl)
		end
		if t.tag then
			table.insert(buffer, string.format('<%s', t.tag))
			if t.arg then
				for k,v in pairs(t.arg) do
					table.insert(buffer, string.format(' %s="%s"', k, v))
				end
			end
			table.insert(buffer, t.empty and ' />' or '>')
		end
		if t.empty then
			if t.cmd then
				dispatch_command_end(t.cmd, tmpl)
			end
			return
		end
		for k,v in ipairs(t) do
			if 'table' == type(v) then
				c_tag(v)
			elseif 'string' == type(v) then
				local s = string.gsub(v, '%%','%%%%')
				s = string.gsub(s, '${([%w%.]+)}?', function (f)
					table.insert( f_args, f )
					return '%s'
				end)
				table.insert(buffer, s)
			else
				error('There is an error in the representation of the template')
			end
		end
		-- close tag
		if t.tag then
			table.insert(buffer, string.format('</%s>', t.tag))
		end
		if t.cmd then
			compile_chunk(tmpl, buffer, f_args)
			f_args={}
			buffer={}
			dispatch_command_end(t.cmd, tmpl)
		end
		return
	end
	-- start the actual execution
	c_tag(self)
	-- add last chunk to tmpl table
	compile_chunk(tmpl, buffer, f_args)
	table.insert(tmpl, "\treturn concat(x,'')\nend\n")
	table.insert(tmpl, [[
setmetatable(t, {
	-- constructor
	__call = function( self, v )
		local v = v or {}
		if type(v) ~= 'table' then
			error('A Parclates instance constructor expects a table as argument.\n'..
				'expected <table> but got <' .. type(v) ..'>'
			)
		elseif nil ~= v.render then
			error('A Parclates instance cannot contain a key called "render".\n'..
				' "render" has the value '.. v.render
			)
		end
		setmetatable(v, self)
		self.__index    = self
		self.__tostring = r
		-- restrict available functions in the template
		v.print  = print
		v.format = string.format
		v.insert = table.insert
		v.concat = table.concat
		v.pairs  = pairs
		v.ipairs = ipairs
		setfenv(r,v)
		return v
	end
})

return t]])
	local r = table.concat(tmpl,'')
	print(r)
end
Parclate.compile = compile

-- THE EXTRA's
-- a pretty printer, helps to see errors in the table representation
-- can be called by print(instance)
local print_r -- pre-define as local -> called recursively
print_r = function( self, indent, done )
	local cl     = {}
	local done   = done or {}
	local indent = indent or ''
	local nextIndent -- Storage for next indentation value
	for key, value in pairs (self) do
		if type (value) == 'table' and not done [value] then
			nextIndent = nextIndent or
				(indent .. string.rep(' ',string.len(tostring (key))+2))
				-- Shortcut conditional allocation
			done [value] = true
			table.insert(cl, indent .. "[" .. tostring (key) .. "] => {\n")
			table.insert(cl, print_r (value, nextIndent, done))
			table.insert(cl, indent .. "}" .. '\n')
		else
			table.insert(cl,
				indent .. "[" .. tostring (key) .. "] => _" .. tostring (value).."_\n"
			)
		end
	end
	return table.concat(cl, '')
end

-- THE CLASS STUFF
-- setup constructor Parclate() and __tostring method
setmetatable(Parclate, {
	-- constructor
	__call = function( self, s )
		if type(s) ~= 'string' then
			error('Parclates constructor expects a string argument.\n'..
				'expected <string> but got <' .. type(s) ..'>'
			)
		elseif not s then
			error('Parclate constructor must be called with an argument!')
		end
		local instance = parse(s)
		setmetatable(instance, self)
		self.__index    = self
		self.__tostring = print_r
		return instance
	end
})

return Parclate
-- vim: ts=4 sw=4 sts=4 sta tw=80 list
