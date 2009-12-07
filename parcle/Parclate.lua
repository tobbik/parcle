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
local parse_args = function ( s )
	local arg = {}
	string.gsub(s, '(%w+)=(["\'])(.-)%2', function (w, _, a)
	     arg[w] = a
	end)
	return arg
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
				table.insert(top, {tag=label, arg=parse_args(xarg), empty=true})
			else
				table.insert(top, {tag=label, empty=true})
			end
		elseif c == '' then     -- start tag
			if '' ~= xarg then
				top = {tag=label, arg=parse_args(xarg), empty=false}
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
-- recursive; serializes one table into (x)html syntax
local s_tag -- pre-define as local -> called recursively
s_tag = function ( t )
	local cl = {}
	if t.tag and t.arg then
		table.insert(cl, string.format('<%s', t.tag))
		for k,v in pairs(t.arg) do
			table.insert(cl, string.format(' %s="%s"', k, v))
		end
		table.insert(cl, ">")
	elseif t.tag then
		table.insert(cl, string.format('<%s>', t.tag))
	end
	for k,v in ipairs(t) do
		if 'table' == type(v) then
			table.insert(cl, s_tag(v))
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
	return table.concat(cl,'')
end
Parclate.serialize = s_tag


-- THE EXTRA's
-- a pretty printer, helps to see errors in the table representation
-- can be called by print(instance)
local print_r -- pre-define as local -> called recursively
print_r = function( t, indent, done )
	local cl     = {}
	local done   = done or {}
	local indent = indent or ''
	local nextIndent -- Storage for next indentation value
	for key, value in pairs (t) do
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
