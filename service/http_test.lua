local z = require "z"
local srv = {}
srv.open = function(sockfd)
					print("open sockfd: " .. sockfd)
				  end

srv.data = function(sockfd, data, sz)

					--print(z.socket_tostring(data, sz))
					--print("data sockfd: " .. sockfd)
					local str = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nhello world!"
					--print(string.len(str))
					--print(str)
					z.socket_send(sockfd, str)
					z.socket_close(sockfd)
				  end

srv.close = function(sockfd)
					print("close sockfd: " .. sockfd)
				 end

local srv_sockfd = nil
local function main()
	local succ, sockfd = z.socket_listen(z.HTTP_PROTO_TYPE, 
										"0.0.0.0", 
										4444);
	if not succ then
		print("start server fail\n")
		return
	end

	local num = 2;
	local agents = {}
	for i=1, num do
		agents[i] = tonumber(z.spawn("zlua", "agent"))	
	end

	local next_agent = 1
	z.socket_dispatch(srv)
	z.socket_start(sockfd, function(newsockfd)
							--z.socket_start(newsockfd)	
							
							z.send("lua", agents[next_agent], 0, "open", newsockfd)
							next_agent = next_agent + 1
							if next_agent > num then
								next_agent = 1
							end
						   end)

	print("start server succ socket fd: " .. sockfd)
end

z.start(function()
			main()
		end)


