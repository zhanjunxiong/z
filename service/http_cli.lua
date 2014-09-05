local z = require "z"

local cli = {
}

cli.open = 
function(sockfd)
	print("open sockfd: " .. sockfd)
	local succ, ret = z.socket_send(sockfd, "hello");
	if not succ then
		print("some errr")
	end
end

cli.data = function(sockfd, data, sz)
					print("sz len: " .. sz)
					print(z.socket_tostring(data, sz))
				   end

cli.close = function(sockfd)
					print("close")
				   end

local function main()
	local succ, sockfd = z.socket_connect(z.HTTP_PROTO_TYPE, 
										"127.0.0.1", 
										4444, 
										0);
	if not succ then
		print("start client fail " .. sockfd)
		return
	end

	z.socket_dispatch(cli)
	z.socket_start(sockfd)

	z.socket_send(sockfd, "hello");
end

z.start(function()
			main()
		end)


