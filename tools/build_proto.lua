local dir = require "dir"
local config = require "config"

local proto_path = "proto"

local function build_pb(root_path, save_path)
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
					print(cmd)
					os.execute(cmd)
				end
			end
		end
	end
end


local function main(...)
	local path = config.proto_path
	local pb_path = config.pb_path

	build_pb(path, pb_path)
	build_pb(path .. "/playerdata", pb_path .. "/playerdata")

	os.exit(0)
end

main()

