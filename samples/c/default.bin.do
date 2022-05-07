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

local f = io.open(RName)

local RList, DList

if f then
  local RNames = {}
  for n in f:lines() do
    local s = n
    if n:match("%.o$") then
      s = Sanitize(TDir.. n)
    end
    RNames[s] = true
  end
  f:close()

  local RUniq = {}
  local DUniq = {}
  for n, v in pairs(RNames) do
    if n:match("%.o$") then
      table.insert(RUniq, n)
    else
      table.insert(DUniq, n)
    end
  end
  RList = table.concat(RUniq, " ")
  DList = table.concat(DUniq, " ")
end

if DList and DList ~= "" then
  f = assert(io.popen("pkg-config --libs " .. DList))
  Libs = Libs .. " " .. f:read()
  assert(f:close())
end

if RList and RList ~= "" then
  assert(os.execute("redo-ifchange " .. RList))
  assert(os.execute(Linker .. " -o " .. arg[2] .. " " .. RList .. " " .. Libs))
  assert(os.execute("redo-ifchange " .. arg[2]))
end

