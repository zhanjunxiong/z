local z = require "z"
local pb = require "protobuf"
local loader = require "loader"
local config = require "config"

local seq = 0
local function send(sockfd, seq, cmd, msg)
	return pb.encode(cmd, msg, 
								function(buffer, len)
									local b, blen = z.net_pack(seq, cmd, buffer, len)
									z.socket_send(sockfd, b, blen)
								end)
end


local cli = {}
cli.data = function(sockfd, data, sz)
					local result, seq, cmd, msg, sz = z.net_unpack(data, sz)
					if not result then
						return
					end

					local result, strerr = pb.decode_all(cmd, msg, sz)
					if not result then
						error(string.format("uid(%d) pb decode error (%s)", id, strerr))
					end

					require "tools"
					print(table2str(result))
					send(sockfd, seq, cmd, result)
				   end

cli.close = function(sockfd)
				   end

local function main()

	loader.build_pb()
	loader.load_proto()

	local succ, sockfd = z.socket_connect(z.HEAD_PROTO_TYPE, 
										"127.0.0.1", 
										4444)
	if not succ then
		print("start client fail " .. sockfd)
		return
	end
	z.socket_dispatch(cli)
	z.socket_start(sockfd)
	print("start client succ")

	send(sockfd, seq, "sys.TEST", {});
end

z.start(function()
			main()
		end)


