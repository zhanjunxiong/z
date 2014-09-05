
local function main()
	print("start clear log ...")
	local cmd = string.format("rm -f game/log/*")
	os.execute(cmd)
	print("start clear log succ")
	os.exit(0)
end

main()


