
local function main(...)
	os.execute('valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --leak-check=yes --workaround-gcc296-bugs=yes --log-file=6 -v z fps')
	os.exit(0)
end

main(...)



