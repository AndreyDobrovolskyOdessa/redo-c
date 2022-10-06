#!/usr/bin/env lua

---------- Editable ------------

local Cflags = "-Wall -Wextra"

local Deps = ""

--------------------------------

local Assert = function(cmd, msg)
  local success, how, exit_code = os.execute(cmd)
  if not success then
    if msg then
      io.stderr:write(msg .. "\n")
    end
    os.exit(exit_code)
  end
end

local f

if Deps:match("[^%s]") then
  local TDir, TName = arg[1]:match("(.-)([^/]*)$")

  Assert("redo " .. TDir .. ".redo." .. TName)
  f = assert(io.popen("pkg-config --cflags " .. Deps))
  Cflags = Cflags .. " " .. f:read()
  assert(f:close())
end

f = assert(io.open(arg[3], "w"))
assert(f:write(Cflags .. "\n" .. Deps .. "\n"))
f:close()

