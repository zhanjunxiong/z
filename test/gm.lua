local Player = require"player"

function test_gm()
	local player = Player:new()
	player:gm_login()
	player:call("sys.RELOAD", {
								srv_name = "hall_srv1",
								mod_name = "sys",

								})
	player:close()
	return true
end



