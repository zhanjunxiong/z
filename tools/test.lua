local z = require "z" 

local mod_name
local function show_help()
	print("use 'z test [mod name]'")
end

if select("#", ...) == 0 then
	show_help()
	os.exit(0)
end
mod_name = select(1, ...)

z.dispatch("lua", 
				function(from, session, ...) 
				end)

z.start(function()
		local pid = z.spawn("zlua", "test_srv")	
		assert(pid)
		local ret = z.call("lua", pid, "start_test", mod_name) 
		if ret then
			print("test succ")
		else
			print("test fail")
		end
		os.exit()
	end)



