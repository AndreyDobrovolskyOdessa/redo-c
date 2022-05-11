#!/usr/bin/env lua

-- The purpose of this script is to compile "arg[2].c", mark all
-- immediate dependencies with "redo-ifchange" and collect all
-- recursive dependencies in "arg[2].require"

--------- Editable ---------

local Compiler = "gcc"

----------------------------


local Sanitize = function(P)
  local D, N = P:match("(.-)([^/]*)$")

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


local CName = arg[2] .. ".c"
local OName = arg[2] .. ".o"

local TDir, TName = arg[2]:match("(.-)([^/]*)$")

local FName = TDir .. "local.cflags"

assert(os.execute("redo-ifchange " .. FName))

local f = assert(io.open(FName))
local Cflags = assert(f:read())
local Deps = assert(f:read())
f:close()


local INames = {}
local RNames = {}
local RNamesShort = {}


local DName = arg[2] .. ".d"

assert(os.execute(Compiler .. " -MD " .. Cflags .. " -o " .. OName .. " -c " .. CName))

local f = assert(io.open(DName))

for l in f:lines() do
  for w in l:gmatch("[^%s]+") do
    local First = w:sub(1,1)
    local Last = w:sub(-1)
    if First ~= "/" and (not ([[:c\]]):find(Last)) then
      table.insert(INames, w)
      local wb = w:match("(.*)%.h$")
      if wb and w ~= arg[2] .. ".h" then
        if os.execute("test -e " .. wb .. ".c") then
          local r = wb .. ".require"
          table.insert(RNames, r)
          table.insert(RNamesShort, r:sub(#TDir + 1))
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
  local fr = assert(io.open(rn))
  for r in fr:lines() do
    local n = r
    if r:match("%.o$") then
      n = Sanitize(RDir .. r)
    end
    DepNames[n] = true
  end
  fr:close()
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

