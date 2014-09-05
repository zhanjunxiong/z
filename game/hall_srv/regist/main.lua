--
--@msg #regist.CREATE_ROLE
function CREATE_ROLE(player, cmd, seq, msg)
	print("create_role")
	local uname = msg.uname
	player.data.user = player.data.user or {}
	local user = player.data.user
	--@user #playerdata.user.User
	user.name = uname
	user.uid = player.uid
	
	msg.errno = 0
	return msg
end


