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

local function parse_args (s)
	local arg = {}
	string.gsub(s, "(%w+)=([\"'])(.-)%2", function (w, _, a)
	     arg[w] = a
	end)
	return arg
end

local function parse(s)
	local stack = {}
	local top = {}
	table.insert(stack, top)
	local ni,c,label,xarg, empty
	local i, j = 1, 1
	while true do
		ni,j,c,label,xarg, empty = string.find(s, "<(%/?)([%w:]+)(.-)(%/?)>", i)
		if not ni then break end
		local text = string.sub(s, i, ni-1)
		if not string.find(text, "^%s*$") then
			table.insert(top, text)
		end
		if empty == "/" then    -- empty element tag
			table.insert(top, {label=label, xarg=parse_args(xarg), empty=1})
		elseif c == "" then     -- start tag
			top = {label=label, xarg=parse_args(xarg)}
			table.insert(stack, top)     -- new level
		else                    -- end tag
			local toclose = table.remove(stack)  -- remove top
			top = stack[#stack]
			if #stack < 1 then
				error("nothing to close with "..label)
			end
			if toclose.label ~= label then
				error("trying to close "..toclose.label.." with "..label)
			end
			table.insert(top, toclose)
		end
		i = j+1
	end
	local text = string.sub(s, i)
	if not string.find(text, "^%s*$") then
		table.insert(stack[#stack], text)
	end
	if #stack > 1 then
		error("unclosed "..stack[stack.n].label)
	end
	return stack[1]
end

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
Parclate.print_r = print_r

-- constructor
local new = function(self, s)
	if type(s) ~= 'string' then
		error('Parclate\s constructor expects a string argument.\n'..
			'expected <string> but got <' .. type(s) ..'>'
		)
	elseif not s then
		error('Parclate constructor must be called with an argument!')
	end
	local instance = parse(s)
	setmetatable(instance, self)
	self.__index    = self
	if b then
		balance = b
	end
	return instance
end
Parclate.new = new

return Parclate
-- vim: ts=4 sw=4 sts=4 sta tw=80 list
