--[[
-- usage:
-- 	 need spawn log server first
-- 	 	z.spawn("logger", ...)
-- 	 	z.regist(log_srv_name)
--]]
--
local z = require "z"

local log_srv_name = "logsrv"

-- 日志等级类型
-- see src/define.h
local MSG_TYPE_LOG_ERROR = 0
local MSG_TYPE_LOG_WARN = 1
local MSG_TYPE_LOG_LOG = 2
local MSG_TYPE_LOG_MSG = 3
local MSG_TYPE_LOG_DEBUG = 4
local MSG_TYPE_LOG_STATE = 5
local MSG_TYPE_LOG_LEN = 6

local Logger = {
}
Logger.__index = Logger

local log_srv_pid = nil
local function get_pid()
	if log_srv_pid then
		return log_srv_pid
	end
	local pid = z.get_pid(log_srv_name)
	log_srv_pid = pid
	return pid
end

function Logger:print(log_level, text)
	text = string.format("[%s][%s] %s", 
				self.from, 
				self.modname,
				text)
	z.redirect(0, get_pid(), 0, log_level, text)
end

function Logger:error(text)
	self:print(MSG_TYPE_LOG_ERROR, text)
end

function Logger:warn(text)
	self:print(MSG_TYPE_LOG_WARN, text)
end

function Logger:log(text)
	self:print(MSG_TYPE_LOG_LOG, text)
end

function Logger:msg(text)
	self:print(MSG_TYPE_LOG_MSG, text)
end

function Logger:debug(text)
	self:print(MSG_TYPE_LOG_DEBUG, text)
end

local M = {}
function M.get_logger(from, modname)
	local logger = {from=from, modname=modname}
	setmetatable(logger, Logger)
	return logger
end

return M

