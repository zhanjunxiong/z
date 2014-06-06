
local z = require "z"
local tcpserver = require "tcpserver"
local pb = require "protobuf"
local loader = require "loader"

local proto_name = "lua"
local hall_srv = "hall_srv"

local srv_name = "gateway"
local host = "0.0.0.0"
local port = 4444

-- load proto
loader.load_proto()

-- load hall_srv
z.load_service(hall_srv, "hall_srv")

function on_connection(id)
	z.send(proto_name, hall_srv, "open", id) 
end

function on_message(id, protoname, cmd, seq, msg, sz)
	local result, strerr = pb.decode(cmd, msg, sz)
	if not result then
		error(string.format("uid(%d) pb decode error (%s)", id, strerr))
	end
	z.send(proto_name, hall_srv, "data", id, seq, cmd, result)
end

function on_close(id)
	z.send(proto_name, hall_srv, "close", id) 
end

local srv = tcpserver.create(srv_name, host, port)
srv:set_proto("pb")
srv:connection_callback(on_connection)
srv:close_callback(on_close)
srv:protomessage_callback(on_message)
srv:start()


