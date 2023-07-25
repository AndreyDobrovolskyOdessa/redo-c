# Parallel builds examples

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


## mk.lua

Utility for creating test projects. Accepts thr next CLI parameters:

* $1 - number of nodes in project. Default is 30.
* $2 - number of sources. Default is 10. Sources are named `t<N>.src`. Other nodes (targets) are named `t<N>`.
* $3 - maximum number of dependences per node. Default is 5. Actual dependences number is random in the range [1 ... $3].
* $4 - dependences window. Default is $2 + $3. Increase nesting of the project tree. Must be greater or equal than $2.

Each target script implements random delay in the range [0s ... 1s].

## mktest

    . ./redo.do
    cd samples/parallel/mktest
    lua ../mk.lua

produce files for project `t` in `mktest` directory.


Conventional build

    redo t


Cleaning sources

    rm -f *.src


First parallel build pass will be sequential and will collect the build tree log:

    redo t.parallel

You will see warning messages about missing roadmap. The next builds:

    redo t.parallel

will be parallel utilizing default JOBS=2 variable. You can increase the number of parallel branches assigning desired JOBS value, for example:

    JOBS=8 redo t.parallel


Full clean the test directory for another instance testing:

    rm -f t* *.do .do..*
    lua ../mk.lua 100 20 10 20


## mktest2

An example of managing logs in massively parallel build

