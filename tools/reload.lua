
local client = require "client"
local config = require "config"

local gm_srv_config = config.gm_srv
assert(gm_srv_config)

local gm_srv_name = gm_srv_config.name
local gm_srv_host = gm_srv_config.host
local gm_srv_port = gm_srv_config.port
local gm_srv_addr = gm_srv_config.srv_addr

local cli_name="test"
local srv_addr = "127.0.0.1"
local srv_port = 5555

local succ, err = pcall(client.start,
		cli_name, 
		srv_addr, 
		srv_port, 

		function(cli)
			local succ, _, result = call(cli, "sys.TEST", {})
			if not succ then
				os.exit(1)
			end

			-- exit program
			os.exit(0)
		end
	 )
if not succ then
	print(tostring(err))
	os.exit(1)
end
