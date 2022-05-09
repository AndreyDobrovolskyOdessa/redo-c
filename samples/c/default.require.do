#!/usr/bin/env lua


--------- Editable ---------

local Compiler = "gcc"

----------------------------


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


local BName = arg[1]:gsub("%.require$", "") -- = arg[2]

local CName = BName .. ".c"
local OName = BName .. ".o"

local TDir, TName = Split(BName)

local FName = TDir .. "local.cflags"

assert(os.execute("redo-ifchange " .. FName))

local f = assert(io.open(FName))
local Cflags = assert(f:read())
local Deps = assert(f:read())
f:close()


local INames = {}
local RNames = {}
local RNamesShort = {}


local DName = BName .. ".d"

assert(os.execute(Compiler .. " -MD " .. Cflags .. " -o " .. OName .. " -c " .. CName))

local f = assert(io.open(DName))

for l in f:lines() do
  for w in l:gmatch("[^%s]+") do
    local First = w:sub(1,1)
    local Last = w:sub(-1)
    if First ~= "/" and (not ([[:c\]]):find(Last)) then
      table.insert(INames, w)
      if w:match(".*%.h") and w ~= BName .. ".h" then
        local wb = w:gsub("%.h$", "")
        local c = wb .. ".c"
        if os.execute("test -e " .. c) then
          local r = wb .. ".require"
          table.insert(RNames, r)
          table.insert(RNamesShort, (r:gsub(TDir, "", 1)))
        end
      end
    end
  end
end

f:close()

os.execute("rm " .. DName)

local IList = table.concat(INames, " ")
local RList = table.concat(RNames, " ")
local AllDeps = table.concat({CName, OName, IList, RList}, " ")

assert(os.execute("redo-ifchange " .. AllDeps))


local DepNames = {}

DepNames[TName .. ".o"] = true

for d in Deps:gmatch("[^%s]+") do
  DepNames[d] = true
end


for i, rn in ipairs(RNames) do
  local RDir = RNamesShort[i]:match(".*/") or ""
  local fr = io.open(rn)
  if fr then
    for r in fr:lines() do
      local n = ""
      if r:match("%.o$") then
        n = RDir
      end
      n = Sanitize(n .. r)
      DepNames[n] = true
    end
    fr:close()
  end
end


local SortedNames = {}

for n, v in pairs(DepNames) do
  table.insert(SortedNames, n)
end

table.sort(SortedNames)


f = io.open(arg[3], "w")

for i, n in ipairs(SortedNames) do
  f:write(n, "\n")
end

f:close()

