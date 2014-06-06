local config = require "config"
require "dir"
local parser = require "parser"

local function register_proto(root_path)
	for fname, ftype in dir(root_path) do
		if fname == "." or fname == ".." then
		else
			if ftype == "file" then
				if string.find(fname, ".proto$") then
					parser.register(fname, root_path) 
				end
			else
				register_proto(root_path .. "/" .. fname)
			end
		end
	end
end

local function load_mod(root_path)
	for fname, ftype in dir(root_path) do
		if fname == "." or fname == ".." then
		else
			if ftype == "file" then
				if string.find(fname, ".lua$") then
					local f, msg = loadfile(root_path .. "/" .. fname)	
					if not f then
						return false, msg
					else
						f()
					end
				end
			else
				load_mod(root_path .. "/" .. fname)
			end
		end
	end
	return true
end

local loader = {}
function loader.load_proto()
	local proto_path = assert(config.proto_path)
	register_proto(proto_path)
end

function loader.load_mods(path)
	local ret, msg = load_mod(path)
	return ret, msg
end

return loader

