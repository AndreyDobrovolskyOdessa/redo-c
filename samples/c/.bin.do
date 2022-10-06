#!/usr/bin/env lua

--------- Editable -----------

local Linker = "gcc"

local Libs = ""

------------------------------

local BinName = arg[2]

local CName = arg[2] .. ".c"
local RName = arg[2] .. ".require"

-- if arg[2] == "main" then current directory name
-- is used as the output binary name

local TDir, TName = arg[2]:match("(.-)([^/]*)$")

if TName == "main" then
  local DirName = TDir
  if DirName == "" then
    local f = assert(io.popen("pwd"))
    DirName = f:read() .. "/"
    assert(f:close())
  end
  BinName = TDir .. DirName:match("([^/]*)/$")
end


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

Assert("redo " .. RName)

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

Assert("redo " .. RList)
Assert(Linker .. " -o " .. BinName .. " " .. RList .. " " .. Libs)
Assert("redo " .. BinName)

