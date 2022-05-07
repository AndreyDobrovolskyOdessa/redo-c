#!/usr/bin/env lua

-- Default .do file for '' target
-- Starts build of the binary with the name equal to directory name

local TargetDir = arg[1]

if TargetDir == "" then
  local f = io.popen("pwd")
  TargetDir = f:read() .. "/"
  f:close()
end

local DirName = TargetDir:match("([^/]*)/$")

if not os.execute("redo-ifchange " .. TargetDir .. DirName .. ".bin") then
  os.exit(1)
end

