# Parallel builds examples

## Simple

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/simple
    rm .do..*
    echo 1 > seed
    time redo sequential
    echo 2 > seed
    time redo sequential
    rm .do..*
    echo 1 > seed
    time redo parallel
    echo 2 > seed
    time redo parallel


## Complex

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/complex
    rm .do..*
    echo 1 > seed
    time redo sequential
    echo 2 > seed
    time redo sequential
    rm .do..*
    echo 1 > seed
    time redo parallel
    echo 2 > seed
    time redo parallel

## Loop

This example demonstrates how lock-free approach allows to locate and avoid loop dependencies even in parallel builds.

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/loop
    redo parallel

## Roadmap

Roadmap file contains the numeric representation of the build tree along with the target names (absolute or relative). The project build log(s) may be transponded into the roadmap file with the help of `log2map.lua` utility.

    lua log2map.lua logfile > roadmap

for absolute target names or

    MAP_DIR=$(pwd) lua log2map.lua logfile > roadmap

to write the roadmap with the relative target names.

An example of roadmap usage is shown in `.parallel.do`. If `some.target` is built with

    redo some.target

then it may be built in parallel with

    JOBS=4 redo some.target.parallel

Default is JOBS=2. `some.target.parallel` will appear to be the roadmap file.
 
