local dir = require "dir"
local protobuf = require "protobuf"

local function register_proto(root_path)
	for fname, ftype in dir.open(root_path) do
		if fname == "." or fname == ".." then
		else
			if ftype == "file" then
				if string.find(fname, ".pb$") then
					local file = string.format("%s/%s", root_path, fname)
					--print("file: " .. file)
					addr = io.open(file, "rb")
					buffer = addr:read("*all")
					addr:close()
					protobuf.register(buffer) 
				end
			else
				register_proto(root_path .. "/" .. fname)
			end
		end
	end
end

local function build_pb(proto_path, root_path, save_path)
	for fname, ftype in dir.open(root_path) do
		if fname == "." or fname == ".." then
		else
			if ftype == "file" then
				local pos = string.find(fname, ".proto$")
				if pos then
					local pb_file = string.format("%s/%s", 
												save_path, 
												string.sub(fname, 1, pos))
					local cmd = string.format("protoc -o%spb %s --proto_path=%s --include_imports", 
									pb_file, 
									root_path .. "/" .. fname,
									proto_path)
					--print(cmd)
					os.execute(cmd)
				end
			end
		end
	end
end

local loader = {}
function loader.load_proto(pb_path)
	pb_path = pb_path or "pb"

	local succ, err = pcall(register_proto, pb_path)
	assert(succ, tostring(err))	
end

function loader.build_pb(proto_path, pb_path)
	proto_path = proto_path or "proto"
	pb_path = pb_path or "pb"

	-- build proto to pb
	local t_dir = {}
	for fname, ftype in dir.open(proto_path) do
		if ftype == "dir" then
			if fname == "." or fname == ".." then
			else
				table.insert(t_dir, fname)
			end
		end
	end

	build_pb(proto_path, proto_path, pb_path)
	for _, v in pairs(t_dir) do
		build_pb(proto_path, proto_path .. "/" .. v, pb_path .. "/" .. v)
	end
end

return loader

