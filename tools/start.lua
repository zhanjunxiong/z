local color = require "color"

local print_red = color.print_red
local print_green = color.print_green
local print_blue = color.print_blue

local function main(...)
	local argv = select("#", ...)	
	local program_name = "bootstrap"
	if argv >= 1 then
		program_name = select(1, ...)
	end

	local pid_file = "proc/" .. program_name .. ".pid"
	--print(pid_file)
	local file = io.open(pid_file, "r")
	local pid
	local is_start = true
	if file then
		pid = tonumber(file:read("*a"))
		file:close()
	end

	local cmd = string.format('ps auxw|grep "z %s"|grep -v grep', program_name)
	--print(cmd)
	local cmd_file = io.popen(cmd)
	local str = cmd_file:read("*a")
	cmd_file:close()
	--print(str)
	if str ~= "" then
		is_start = false
	end

	if is_start then
		--print(string.format("start %s ...", program_name))
		local cmd = string.format('z %s %s.log -d -P %s', 
									program_name, 
									program_name, 
									program_name)
		local r = os.execute(cmd)
		print(string.format('Starting %s:	%s[OK]%s', program_name, color.g, color.cn))
	else
		print(string.format('Starting %s: 	%s[OK]%s', program_name, color.r, color.cn))
	end

	os.exit(0)
end

main(...)

