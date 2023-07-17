#!/usr/bin/env lua

local f = io.open("/dev/urandom")

local c = f:read(1):byte()

f:close()

f = io.open(arg[3], "w")

f:write(tostring(c), "\n")

f:close()

