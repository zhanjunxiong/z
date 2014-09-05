
function PING(player, cmd, seq, msg)
	return msg
end

function TEST(player, cmd, seq, msg)
	player:broadcast({1, 2, 3, 4, 5}, "sys.PING", {}) 
	return msg
end


