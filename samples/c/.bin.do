#!/usr/bin/env lua

--------- Editable -----------

local Linker = "gcc"

local Libs = ""

------------------------------

local BinName = arg[2]

local TDir, TName = arg[2]:match("(.-)([^/]*)$")

if TName == "" then
  TName = "main"
  local DirName = TDir
  if DirName == "" then
    local f = assert(io.popen("pwd"))
    DirName = f:read() .. "/"
    assert(f:close())
  end
  BinName = TDir .. DirName:match("([^/]*)/$")
end

local CName = TDir .. TName .. ".c"
local RName = TDir .. TName .. ".require"


local Assert = function(cmd, msg)
  local success, how, exit_code = os.execute(cmd)
  if not success then
    if msg then
      io.stderr:write(msg .. "\n")
    end
    os.exit(exit_code)
  end
end


Assert("test -e " .. CName, "Missing " .. CName)

Assert("depends-on " .. RName)

local RUniq = {}
local DUniq = {}

for n in io.lines(RName) do
  if n:match("%.o$") then
    table.insert(RUniq, TDir .. n)
  else
    table.insert(DUniq, n)
  end
end

local RList = table.concat(RUniq, " ")
local DList = table.concat(DUniq, " ")

if DList ~= "" then
  f = assert(io.popen("pkg-config --libs " .. DList))
  Libs = Libs .. " " .. f:read()
  assert(f:close())
end

Assert("depends-on " .. RList)
Assert(Linker .. " -o " .. BinName .. " " .. RList .. " " .. Libs)
Assert("depends-on " .. BinName)

