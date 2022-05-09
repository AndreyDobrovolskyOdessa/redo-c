#!/usr/bin/env lua


--------- Editable -----------

local Linker = "gcc"

local Libs = ""

------------------------------


local Split = function(P)
  local Dir, Name = P:match("(.*/)(.*)")
  if not Dir then
    Dir = ""
    Name = P
  end
  return Dir, Name
end


local Sanitize = function(P)
  local D, N = Split(P)

  local S = {}

  for part in D:gmatch("([^/]*)/") do
    if part == ".." and #S > 0 and S[#S] ~= ".." then
      table.remove(S)
    else
      table.insert(S, part)
    end
  end

  table.insert(S, "")
  D = table.concat(S, "/")

  return D .. N
end


local TDir, TName = Split(arg[1])

local CName = TDir .. "main.c"

local RName = TDir .. "main.require"

if not os.execute("test -e " .. CName) then
  io.stderr:write("Missing main.c\n")
  os.exit(1)
end

assert(os.execute("redo-ifchange " .. RName))

local f = assert(io.open(RName))

local RUniq = {}
local DUniq = {}

for n in f:lines() do
  if n:match("%.o$") then
    local s = Sanitize(TDir .. n)
    table.insert(RUniq, s)
  else
    table.insert(DUniq, n)
  end
end

f:close()

local RList = table.concat(RUniq, " ")
local DList = table.concat(DUniq, " ")

if DList ~= "" then
  f = assert(io.popen("pkg-config --libs " .. DList))
  Libs = Libs .. " " .. f:read()
  assert(f:close())
end

assert(os.execute("redo-ifchange " .. RList))
assert(os.execute(Linker .. " -o " .. arg[2] .. " " .. RList .. " " .. Libs))
assert(os.execute("redo-ifchange " .. arg[2]))

