-- import Lua built in dependencies
local string = require( 'string' )
local table  = require( 'table'  )

local pairs, setmetatable, tostring, type , unpack =
      pairs, setmetatable, tostring, type , unpack

-- determine version
-- We use that to determine if "setfenv" or "in env do"
local v52 = (_VERSION=='Lua 5.2') and true or false

--[[_   __ _ _ __ ___  ___
| '_ \ / _` | '__/ __|/ _ \
| |_) | (_| | |  \__ \  __/
| .__/ \__,_|_|  |___/\___|
|_|  --]]
-- helper to disect arguments of tags
local parse_args = function ( s, tag, empty )
	local args   = {}
	local cmds   = {}
	local tag_t  = {tag=tag, empty=empty}
	string.gsub(s, '([^%s="]+)=(["\'])(.-)%2', function (w, _, a)
		local ns,cmd = string.match(w,'^(l):(%w+)')
		if ns and cmd then
			if     'strip' == cmd then
				tag_t['strip'] = true
			elseif 'attrs' == cmd then
				tag_t['attrs'] = a
			else
				cmds[cmd] = a
			end
		else
			args[w]   = a
		end
	end)
	if next(cmds) then
		tag_t['cmd'] = cmds
	end
	if next(args) then
		tag_t['arg'] = args
	end
	return tag_t
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

--[[           _       _ _
 ___  ___ _ __(_) __ _| (_)_______
/ __|/ _ \ '__| |/ _` | | |_  / _ \
\__ \  __/ |  | | (_| | | |/ /  __/
|___/\___|_|  |_|\__,_|_|_/___\___| --]]
-- serializes the representation back into (x)html template
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
			if t.strip then table.insert(cl, ' l:strip=""') end
			if t.attrs then
				table.insert(cl, string.format(' l:attrs="%s"', t.attrs))
			end
			if t.cmd then
				for c,e in pairs(t.cmd) do
					table.insert(cl, string.format(' l:%s="%s"', c, e))
				end
			end
			table.insert(cl, t.empty and ' />' or '>')
			if t.empty then return end
		end
		for i=1,#t do
			local v=t[i]
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

--[[         __ _ _
| |_ ___    / _(_) | ___
| __/ _ \  | |_| | |/ _ \
| || (_) | |  _| | |  __/
 \__\___/  |_| |_|_|\___|--]]
-- #private: create lua function source code from the arguments
local compile_command = function(cmd, c_buf)
	for c,exp in pairs(cmd) do
		if 'if' == c then
			table.insert(c_buf, string.format('\tif %s then\n\t\t', exp) )
		elseif 'for' == c then
			table.insert(c_buf, string.format('\tfor %s do\n\t\t', exp) )
		end
	end
end

-- #private: helper to close open functions in the compiled chunk
local compile_command_end = function(cmd, c_buf)
	for c,_ in pairs(cmd) do
		if 'if' == c or 'for' == c then
			table.insert(c_buf, '\tend\n')
		end
	end
end

-- #private: combine the last set of adjacent strings into one chunk
local compile_buffer = function(c_buf, buffer, f_args, cnt)
	if 0==#buffer and 0==#f_args then return cnt end
	if 0 == #f_args then
		table.insert(c_buf,
			' insert(x,[[' .. table.concat(buffer,'') .. ']])\n'
		)
	else
		table.insert(c_buf,
			' insert(x, format([[' ..
				table.concat(buffer,'') ..
			']],'.. table.concat(f_args, ',') ..'))\n'
		)
		-- f_args={} creates a new reference, table.remove preserves it
		while #f_args>0 do table.remove(f_args) end
	end
	while #buffer>0 do table.remove(buffer) end
	return cnt+1
end

-- #private: renders just the chunk, that actully flushes the template
-- @return: table, that needs to be table.concat()
local compile_chunk = function (r)
	local c_buf  = {}    -- buffer that holds entire chunk
	local buffer = {}    -- buffer that holds last set of adjacent string
	local f_args = {}    -- holds the string.format arguments for "buffer"
	local chunk_cnt = 0  -- helps to determine initial length of buffer table
	local c_tag          -- predeclare local for recursive calls
	c_tag = function(t)
		if t.cmd then
			chunk_cnt = compile_buffer(c_buf, buffer, f_args, chunk_cnt)
			compile_command(t.cmd, c_buf)
		end
		if t.tag and not t.strip then
			table.insert(buffer, string.format('<%s', t.tag))
			if t.arg then
				for k,v in pairs(t.arg) do
					table.insert(buffer, string.format(' %s="%s"', k, v))
				end
			end
			if t.attrs then
				chunk_cnt = compile_buffer(c_buf, buffer, f_args, chunk_cnt)
				table.insert(c_buf, string.format(
					"\tfor _at,_atv in pairs(%s) do\n" ..
						"\t\tinsert(x, format([=[ %%s=\"%%s\"]=], _at, _atv))\n" ..
					"\tend\n", t.attrs))
				chunk_cnt = chunk_cnt+1
			end
			table.insert(buffer, t.empty and ' />' or '>')
		end
		if t.empty then
			if t.cmd then
				compile_command_end(t.cmd, c_buf)
			end
			return
		end
		for i=1,#t do
			local v=t[i]
			if 'table' == type(v) then
				c_tag(v)
			elseif 'string' == type(v) then
				local s = string.gsub(v, '%%','%%%%')
				s = string.gsub(s, '${([^}]+)}?', function (f)
					table.insert( f_args, f )
					return '%s'
				end)
				table.insert(buffer, s)
			else
				error('There is an error in the representation of the template')
			end
		end
		-- close tag
		if t.tag and not t.strip then
			table.insert(buffer, string.format('</%s>', t.tag))
		end
		if t.cmd then
			chunk_cnt = compile_buffer(c_buf, buffer, f_args, chunk_cnt)
			compile_command_end(t.cmd, c_buf)
		end
		return
	end
	-- start the actual execution
	c_tag(r)
	-- add last chunk to c_buf table
	chunk_cnt = compile_buffer(c_buf, buffer, f_args, chunk_cnt)
	table.insert(c_buf, " return concat(x,'')")
	table.insert(c_buf,1,'}\n')
	-- prefill the array with a safe amount of empty slots to avoid rehashing
	--   - excessive 'for' loops will make that less useful
	local empties = 1+math.pow(2, (math.ceil(math.log(chunk_cnt)/math.log(2))+1))
	for i=1,empties do
		local addy = (i==1) and "''" or "'',"
		table.insert(c_buf,1,addy)
	end
	table.insert(c_buf,1,' local x={')
	return c_buf
end

-- #public: generate the string for a file which is a compiled template
local to_file = function(self)
	if v52 then
		return string.format(
[[local f={format=string.format,pairs=pairs,ipairs=ipairs,
 concat=table.concat,insert=table.insert,tostring=tostring}
return setmetatable({}, {
 __tostring=function( _ENV) %s end,
 __call=function(s) for k,v in pairs(s) do if not f[k] then s[k]=nil end end end,
 __index=function(s,k) if 'nil'==type(f[k]) then return rawget(s,k)
                       else                      return rawget(f,k) end end,
 __newindex=function(s,k,v) if 'nil'==type(f[k]) then rawset(s,k,v)
       else error("<"..k.."> cannot be set on the table" ) end end})]],
		table.concat(compile_chunk(self)) )
	else
		return string.format(
[[local t,f={},{format=string.format,pairs=pairs,ipairs=ipairs,
 concat=table.concat,insert=table.insert,tostring=tostring}
return setmetatable(t,{
 __tostring=setfenv(function() %s end,t),
 __call=function(s) for k,v in pairs(s) do if not f[k] then s[k]=nil end end end,
 __index=function(s,k) if 'nil'==type(f[k]) then return rawget(s,k)
                       else                      return rawget(f,k) end end,
 __newindex=function(s,k,v) if 'nil'==type(f[k]) then rawset(s,k,v)
       else error("<"..k.."> cannot be set on the table" ) end end})]],
	table.concat(compile_chunk(self)) )
	end
end

--[[                       _ _
  ___ ___  _ __ ___  _ __ (_) | ___
 / __/ _ \| '_ ` _ \| '_ \| | |/ _ \
| (_| (_) | | | | | | |_) | | |  __/
 \___\___/|_| |_| |_| .__/|_|_|\___|
                    |_|              --]]
-- #public: create a table that represents just the compiled template
local compile = function(self)
	-- holds all the protected immutable functions of the template
	local funcs = {
		format = string.format, pairs = pairs, ipairs = ipairs,
		concat = table.concat,  insert = table.insert, tostring = tostring
	}
	-- the mutable part of the template that holds the values
	local t = {}
	-- prepare the compiled chunk (render function) and set into environment
	local chunk = (v52) and
		assert( loadin(t, table.concat(compile_chunk(self)) ))
		or
		setfenv(assert( loadstring(table.concat(compile_chunk(self)) )), t)
	return setmetatable(t, {
		__tostring = chunk,
		-- does flushing it really makes sense?
		__call     = function(self)
			for k,v in pairs(self) do
				if not funcs[k] then
					self[k] = nil
				end
			end
		end,
		-- access functions from the funcs table, user (template) variables from
		-- the self instance
		__index    = function(self,k)
			if 'nil' ~= type(funcs[k]) then
				return rawget( funcs, k )
			else
				return rawget( self, k )
			end
		end,
		-- protect functions in the funcs table, don't allow identically named
		-- vars in the instance table
		__newindex = function(self, k, v)
			if 'nil' ~= type(funcs[k]) then
				error("<"..k.."> cannot be set on the template" )
			else
				rawset( self, k, v )
			end
		end
	})
end

-- THE EXTRA's
-- a pretty printer, helps to see errors in the table representation
-- can be called by print(instance)
local cmp=function(a,b)
	if type(a)~=type(b) then
		return tostring(a)>tostring(b)
	else
		return a<b
	end
end
local print_r -- pre-define as local -> called recursively
print_r = function( self, indent, done )
	local cl     = {}
	local done   = done or {}
	local indent = indent or ''
	local s_index= {}
	for key, value in pairs (self) do
		table.insert(s_index,key)
	end
	table.sort(s_index,cmp)
	local nextIndent -- Storage for next indentation value
	-- deal with the keys,args and commands first
	for i,key in pairs (s_index) do
		local value = self[key]
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
				indent .. "[" .. tostring (key) .. "] => '" .. tostring (value).."'\n"
			)
		end
	end
	return table.concat(cl, '')
end

-- THE CLASS STUFF
-- setup constructor ...() and __tostring method
return setmetatable({
			serialize = serialize,
			to_file   = to_file,
			compile   = compile,
			debug     = print_r
		}, {
	-- constructor
	__call = function( self, s )
		if type(s) ~= 'string' then
			error('Parclates constructor expects a string argument.\n'..
				'expected <string> but got <' .. type(s) ..'>'
			)
		elseif not s then
			error('Parclate constructor must be called with an argument!')
		end
		self.__index    = self
		self.__tostring = to_file
		self.__call     = compile
		return setmetatable(parse(s), self)
	end
})

-- vim: ts=4 sw=4 sts=4 sta tw=80 list
