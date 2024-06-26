#!/usr/bin/env lua

-- The purpose of this script is to compile "arg[2].c", mark all
-- immediate dependencies with "depends-on" and collect all recursive
-- dependencies in "arg[2].require"

--------- Editable ---------

local Compiler = "tcc"

----------------------------

local CName = arg[2] .. ".c"
local HName = arg[2] .. ".h"
local OName = arg[2] .. ".o"
local DName = arg[2] .. ".d"

local TDir, TName = arg[2]:match("(.-)([^/]*)$")

local TNameStart = #TDir + 1

local FName = TDir .. "local.cflags"


local Assert = function(cmd, msg)
  local success, how, exit_code = os.execute(cmd)
  if not success then
    if msg then
      io.stderr:write(msg .. "\n")
    end
    os.exit(exit_code)
  end
end


Assert("depends-on " .. FName)

local f = assert(io.open(FName))
local Cflags = assert(f:read())
local Deps = assert(f:read())
f:close()

Assert(Compiler .. " -MD " .. Cflags .. " -o " .. OName .. " -c " .. CName)


local INames = {}
local RNames = {}

for l in io.lines(DName) do
  for w in l:gmatch("[^%s]+") do
    local First = w:sub(1,1)
    local Last = w:sub(-1)
    if First ~= "/" and (not ([[:c\]]):find(Last)) then
      table.insert(INames, w)
      local wb = w:match("(.*)%.h$")
      if wb and w ~= HName and os.execute("test -e " .. wb .. ".c") then
        table.insert(RNames, wb .. ".require")
      end
    end
  end
end

os.execute("rm " .. DName)

local IList = table.concat(INames, " ")
local RList = table.concat(RNames, " ")
local AllDeps = table.concat({CName, OName, IList, RList}, " ")

Assert("depends-on " .. AllDeps)


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


local DepNames = {}

DepNames[TName .. ".o"] = true

for d in Deps:gmatch("[^%s]+") do
  DepNames[d] = true
end

for i, rn in ipairs(RNames) do
  local RDir = rn:match(".*/", TNameStart) or ""
  local fr = io.open(rn)
  if fr then
    for r in fr:lines() do
      local n = r:match("%.o$") and Sanitize(RDir .. r) or r
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

f = assert(io.open(arg[3], "w"))
assert(f:write(table.concat(SortedNames, "\n")))
f:close()

