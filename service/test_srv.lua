local z = require "z"
local loader = require "loader"
local config = require "config"
local color = require "color"

local print_red = color.print_red
local print_green = color.print_green
local print_blue = color.print_blue

local test_cases = {}
local test_result = 0
local test_succ = 0
local test_fail = 0

--@param testcase
--			{
--			name = 'xxx',
--			func = function() ... end
--			}
local function regist_testcase(testcase)
	test_cases[#test_cases + 1] = testcase
	test_result = test_result + 1
end

local function run_testcase()
	for _, testcase in pairs(test_cases) do
		print("======================")
		print_blue("Run TestCase:" .. testcase.name)
		local succ, ret = pcall(testcase.func)
		if not succ then
			print(tostring(ret))	
		end
		print_blue("End TestCase:" .. testcase.name)

		if ret then
			test_succ = test_succ + 1
		else
			test_fail = test_fail + 1
		end
	end

	print("Total TestCase: " .. test_succ + test_fail)
	print_green("Passed: " .. test_succ)
	print_red("Failed: " .. test_fail)
end

function print_r(obj, indent)
	local obj_type = type(obj)
	if obj_type == "number" then
		print(obj)
		return
	end

	if obj_type == "string" then
		print(string.format("%q", obj))
		return
	end

	indent = indent or 0
	if obj_type == "table" then
		for k, v in pairs(obj) do
			if type(k) == "string" then
				k = string.format("%q", k)
			end
			local suffix = ""
			if type(v) == "table" then
				suffix = "{"
			end
			local prefix = string.rep("\t", indent)
			formatting = prefix .. "[" .. k .. "]" .. "=" .. suffix
			if type(v) == "table" then
				print(formatting)
				print_r(v, indent + 1)
				print(prefix .. "},")
			else
				local value = ""
				if type(v) == "string" then
					value = string.format("%q", v)
				else
					value = tostring(v)
				end
				print(formatting .. value .. ",")
			end
		end
	end
end

function expect_eq(m, n)
	local m_type = type(m)
	local n_type = type(n)
	local ret = false
	if m_type == n_type then
		if m_type == 'number' or m_type == 'string' then
			if m == n then
				ret = true
			end
		elseif m_type == 'table' then
			ret = true
			for k, v in pairs(m) do
				if n[k] ~= v then
					ret = false
					break
				end
			end
		end
	end

	if not ret then
		print_red("Failed")
		print_red("Expect: " .. " type(m):" .. m_type)
		print_r(m)	
		print_red("Actual: " .. " type(n):" .. n_type)
		print_r(n)
	end
	return true
end

local function load_mod(path, mod_name)
	local mod = {}
	setmetatable(mod, {__index = _G})
	local func, err = loadfile(path .. "/" .. mod_name, "bt", mod)
	if not func then
		print_red(string.format("load mod(%s) fail, err(%s)", mod_name, err))
		return nil
	end
	func()
	return mod
end

local CMD = {}
function CMD.start_test(file_name)
	local file_name = string.gsub(file_name, ".lua", "") .. ".lua"

	local mod = load_mod("test", file_name)
	if not mod then
		return false
	end

	for k, v in pairs(mod) do
		if type(v) == "function" and 
			string.sub(k, 1, 5) == "test_" then
			regist_testcase({name=k, func=v})
		end
	end

	local succ, err = pcall(run_testcase)
	if not succ then
		print(tostring(err))	
		return false
	end
	return true
end

function CMD.close()
	print("player close")
	os.exit()
end

z.dispatch("lua", function(from, session, cmd, ...)
						local f = CMD[cmd]
						assert(type(f) == "function")
						z.ret(f(...))
					end)

z.start(function()
		end)

