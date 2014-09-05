local z = require "z"
local pb = require "protobuf"
local loader = require "loader"
local tools = require "tools"

local pid = nil
local socket = nil
local function send(sockfd, seq, cmd, msg)
	return pb.encode(cmd, msg, 
						function(buffer, len)
							local b, blen = z.net_pack(seq, cmd, buffer, len)
							z.socket_send(sockfd, b, blen)
						end)
end


local socket_handle = {}
socket_handle.data = function(sockfd, data, sz)
					local result, seq, cmd, msg, sz = z.net_unpack(data, sz)
					if not result then
						return
					end

					local result, strerr = pb.decode_all(cmd, msg, sz)
					if not result then
						error(string.format("uid(%d) pb decode error (%s)", id, strerr))
					end
					
					if seq > 0 then
						z.rawsend(pid, seq, z.MSG_TYPE_RESPONSE, z.lua_pack(cmd, result))
						return
					end

					print("RECV: " .. table2str(result, cmd))
				 end



socket_handle.close = function(sockfd)
						z.send("lua", pid, "close")
			   		  end


local function socket_connect(srv_addr, srv_port)
	local succ, sockfd = z.socket_connect(z.HEAD_PROTO_TYPE, 
										srv_addr, 
										srv_port)
	if not succ then
		return false
	end

	z.socket_start(sockfd)
	socket = sockfd
	return true
end

local CMD = {}
function CMD.start(from, session, srv_addr, srv_port)
	pid = from
	return socket_connect(srv_addr, srv_port)
end

function CMD.forward(from, session, cmd, msg)
	send(socket, session, cmd, msg)
end

function main()
	-- 加载proto
	loader.load_proto()

	z.socket_dispatch(socket_handle)

end

z.dispatch("lua", function(from, session, cmd, ...)
					local func = CMD[cmd]
					assert(type(func) == 'function')
					z.ret(func(from, session, ...))
				 end)

z.start(function()
			main()
		end)

