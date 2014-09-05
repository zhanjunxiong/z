require "tools"

function test_table2str()
	expect_eq(table2str({1, 2, 3}), "{1, 2, 3}")
end

