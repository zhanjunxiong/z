
local z = require "z"
local pb = require "protobuf"
local tcpclient = require "tcpclient"
local loader = require "loader"

loader.load_proto()

local seq = 0
function send_message(id, cmd, msg)
	pb.encode(cmd, msg, function(buffer, len)

		print("send message")
		local b, blen = z.net_pack(cmd, seq, buffer, len)
		print("send message len: " .. blen)

		z.tcpclient_send("client", b, blen)
		z.net_unpack(b)

		seq = seq + 1
	end)
end

function on_connection(id)
	print("onConnection")
	local login = {
		uid = 12345,
		password = "12345",
	}
	send_message(id, "login.LOGIN", login)
end

function on_message(id, proto_name, cmd, seq, msg, sz)
	local result = pb.decode(cmd, msg, sz)
	if result.errno == -1 then
		print("uid: " .. result.uid .. " login fail" )
	elseif result.errno == 0 then
		print("uid: " .. result.uid .. " login succ")
	end
end

function on_close(id)
	print("onClose, id: " .. id)
end

local name = "client"
local srv_addr = "127.0.0.1"
local srv_port = 4444
client = tcpclient.create(name, srv_addr, srv_port);
client:set_proto("pb")
client:connection_callback(on_connection)
client:protomessage_callback(on_message)
client:close_callback(on_close)
client:start()


