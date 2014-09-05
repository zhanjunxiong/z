local z = require "z" 
local pb = require "protobuf"
local loader = require "loader"
local config = require "config"
local log = require "logger"
local logger = log.get_logger("gm", "gm")

local proto_name = "lua"
require "tools"

local function send(sockfd, seq, cmd, msg)
	return pb.encode(cmd, msg, 
								function(buffer, len)
									local b, blen = z.net_pack(seq, cmd, buffer, len)
									z.socket_send(sockfd, b, blen)
								end)
end

local socket_manager = {}
local function on_message(sockfd, seq, cmd, msg)
	if cmd == "sys.RELOAD" then
		local pid = z.get_pid(msg.srv_name)
		assert(pid)

		local errno = z.call("sys", pid, "reload", msg.mod_name)
		msg.errno = errno
		send(sockfd, seq, cmd, msg)
		return
	end
end

local socket_handle = {}
socket_handle.data = function(sockfd, data, sz)
					local result, seq, cmd, msg, sz	= z.net_unpack(data, sz)
					if not result then
						print("error: " .. seq)
						return
					end

					local result, strerr = pb.decode_all(cmd, msg, sz)
					if not result then
						print(string.format("pb decode error (%s)", strerr))
						return
					end
					
					socket_manager[sockfd] = socket_manager[sockfd] or {}
					on_message(sockfd, seq, cmd, result)
					end

socket_handle.close = function(sockfd)

					local socket = socket_manager[sockfd]
					if not socket then
						return
					end

					socket_manager[sockfd] = nil
				   end


local function main()
	-- 加载proto
	loader.load_proto()

	local succ, sockfd = z.socket_listen(z.HEAD_PROTO_TYPE,
										config.gm_srv.host,
										config.gm_srv.port)
	if not succ then
		logger:log("start server fail")
		return
	end

	z.socket_dispatch(socket_handle)	
	z.dispatch("lua", 
				--@param from 来源地址
				--		 session 服务回话id
				function(from, session, cmd, ...)
					local func = CMD[cmd]
					assert(type(func) == 'function')
					z.ret(func(...))
				end)


	z.socket_start(sockfd, 
						   function(newsockfd)
								z.socket_start(newsockfd)
						   end)

	logger:log(string.format("start gm srv succ, listen on(%s:%d)", 
				config.gm_srv.host,
				config.gm_srv.port))

end

z.start(
	function()
		main()
	end)

