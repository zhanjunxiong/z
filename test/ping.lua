local Player = require"player"

function test_ping()
	local uid = 12345
	local passwd = "12345"
	local player = Player:new()
	player:login(uid, passwd)
	player:call("sys.TEST", {})
	player:close()
	return true
end


