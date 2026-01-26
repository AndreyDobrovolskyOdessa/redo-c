# Using `redo` for parallel builds

This implementation of `redo` is single-thread and parallel-friendly. It means that any number of the `redo` binary instances can successfully co-operate over the same project tree. So the question is how to choose the entry points for `redo` instances.

Let's assume we have some project (the set of recipes) which builds the target `t`.

First of all we need the knowledge of the project tree topology. This `redo` implementation
writes its log in the Lua table format. So

	redo -l t.log t

will build `t` and store the whole project tree in `t.log`. Now we can provide analysis of this tree and recode it into the roadmap format.

## Roadmap

In fact it is the main control structure of this `redo` implementation. It consists of

* array of the nodes' relative names

* arrays of the nodes' childs

* array of the nodes' statuses

Status is an integer indicating the number of unresolved dependencies of the node. Obviously the nodes with status equal to 0 are ready to be build, while those with the positive status are not. `redo` sequentially builds the nodes with 0 status and in case of success marks the node as done with negative status and decrement the statuses of all the node's children. If the node was already built by another `redo` instance then it is simply marked as done.

If `redo` is not supplied with the roadmap it falls back to the list of the targets and uses them to build the trivial roadmap, where every node has no children and its status is initially 0.

Trascoding the log into the roadmap with relative targets' names:

	MAP_DIR=$(pwd) lua log2map.lua t.log > t.roadmap

Now we can build the `t` target with any number of `redo` instances in parallel

	for I in $(seq $JOBS)
	do
		redo -m t.roadmap &
	done
	wait

This code will work nice for the projects with the stable trees. But if the project tree is altering we need to adjust the roadmap after each successful build to keep the build process closer to the optimum. The working example is `samples/parallel/.parallel.do`. It can be applied to any target. If

	redo some-target

builds `some-target` in the single-precess way then

	redo some-target.parallel

will create `some-target.parallel` roadmap at the first run and later use this roadmap to build `some-target` in parallel

	JOBS=8 redo some-target.parallel


### Analogies between `redo` and `Meson/ninja`

	create the project               configure Meson project
	recipes

	redo project.parallel            Meson builddir

	JOBS=8 redo project.parallel     ninja -j 8


## mk.lua

Utility for creating test projects. Accepts the next CLI parameters:

* $1 - number of nodes in project. Default is 30.
* $2 - number of sources. Default is 10. Sources are named `t<N>.src`. Other nodes (targets) are named `t<N>`.
* $3 - maximum number of dependences per node. Default is 5. Actual dependences number is random in the range [1 ... $3].
* $4 - dependences window. Default is $2 + $3. Increase nesting of the project tree. Must be greater or equal than $2.

Each target script implements random delay in the range [0s ... 1s].

## mktest

The playground for parallel builds testing.

    . ./redo.do
    cd samples/parallel/mktest
    lua ../mk.lua

produce files for project `t` in `mktest` directory.


Conventional build

    redo t


Cleaning sources

    rm -f *.src

Resetting build preserving sources and reciepts

    rm -f .do..*


First parallel build pass will be sequential and will collect the build tree log:

    redo t.parallel

The next builds will be parallel utilizing default JOBS=2 variable. You can increase the number of parallel branches assigning desired JOBS value, for example:

    JOBS=8 redo t.parallel


Full clean the test directory for another instance testing:

    rm -f t* *.do .do..*
    lua ../mk.lua 100 20 10 20


## mktest2

An example of managing logs in massively parallel build. An approach described above collects the information about the project tree during initial sequential build. Such an approach is easy to implement because the project recipes has no additional requirements. If You want an initial build to be parallel, the recipes must be written paying additional efforts to handle correctly the log files.

NOTE: this example needs the shell to allow writing into arbitrary file handler, busybox ash for example. Most shells will need some additional utility to accomplish this task.


## Loop

This example demonstrates how lock-free approach allows to locate and avoid loop dependencies even in parallel builds.

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/loop
    redo parallel


