
local function main(...)
	local argv = select("#", ...)
	local program_name = "bootstrap"
	if argv >= 1 then
		program_name = select(1, ...)
	end

	local filename = "proc/" .. program_name .. ".pid"
	local file = io.open(filename, 'r')
	if file then
		local pid = file:read()
		--print(pid)
		print(string.format('start stop %s ...', program_name))
		os.execute('kill -15 ' .. tonumber(pid))
		print(string.format('stop %s succ!', program_name))
	else
		print(string.format('%s not run', program_name))
	end

	os.exit(1)
end

main(...)


