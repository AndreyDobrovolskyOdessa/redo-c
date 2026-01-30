# Using `redo` for parallel builds

This implementation of `redo` is single-thread and parallel-friendly. It means that any number of the `redo` binary instances can successfully co-operate over the same project tree. So the question is how to choose the entry points for `redo` instances.

Let's assume we have some project (the set of recipes) which builds the target `t`.

First of all we need the knowledge of the project tree topology. This `redo` implementation writes its log in the Lua table format. So

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

Log files being used for the roadmap creation are named `some-target.parallel.log` (initial single-thread pass) and `some-target.parallel.N.log` for consequent roadmap-based builds.

By default `stdout` of the build's recipes is being sent on the launcher tty while `stderr` is being stored in the log files. You may use

	QUIET=1 redo some-target.parallel

for storing both standard output files in the log.


### Analogies between `redo` and `Meson/ninja`

	create the project               configure Meson project
	recipes

	redo project.parallel            Meson builddir

	JOBS=8 redo project.parallel     ninja -j 8


## Parallelizing inside the recipes

Obviously makes sence for the targets not having common dependencies. May be used by project developer for the cases of independent targets. An example in shell is `parallel_depends_on()` function, see `samples/parallel/playground/recipe.par`. Function is compatible with `.parallel.do` rule.

## Playground

Equipped with two Lua scrips producing different types of projects - mkmesh.lua and mktree.lua. First produces recipes for the project without limitations on cross dependencies, while the second produces recipes for the project with pure tree topology.


### `mkmesh.lua`

Accepts the next CLI parameters:

* $1 - number of nodes in project. Default is 30.
* $2 - number of sources. Default is 10. Sources are named `t<N>.src`. Other nodes (targets) are named `t<N>`.
* $3 - maximum number of dependencies per node. Default is 5. Actual dependencies number is random in the range [1 ... $3].
* $4 - dependencies window. Default is $2 + $3. Increase nesting of the project tree. Must be greater or equal than $2.

Each target script implements random delay in the range [0s ... 1s].

Main target named `t`.


### `mktree.lua`

* $1 - number of targets. Default is 30.
* $2 - branching coefficient. Default is 3. Greater or equal than 2 and less or equal than 10, float. 

Main target named `t`.


### `recipe.seq` and `recipe.par`

May be engaged in the project with the help of link

	ln -sf recipe.seq recipe

or

	ln -sf recipe.par recipe


### Examples

Supposed to be done once before testing:

	. ./redo.do
	cd samples/parallel/playground


#### Usefull commands

Full clean:

	rm -f t* .do..*

Cleaning sources:

	rm -f *.src

Resetting project preserving sources and recipes:

	rm -f .do..*


#### Example 1

Creating default mesh and build it in plain single-thread manner:

	rm -f t* .do..*
	lua mkmesh.lua
	ln -sf recipe.seq recipe
	redo t

Creating roadmap:

	redo t.parallel

Resetting project and building with the roadmap:

	rm -f .do..*
	JOBS=4 redo t.parallel

#### Example 2

Creating the tree with 50 targets and 200 sources and building in the sequential way:

	rm -f t* .do..*
	lua mktree.lua 50 5
	ln -sf recipe.seq recipe
	redo t

Cleaning and building the same project using in-recipe parallelizing:

	rm -f .do..*
	ln -sf recipe.par recipe
	redo t

Creating roadmap:

	redo t.parallel

Resetting project and building with the roadmap:

	rm -f .do..*
	JOBS=4 QUIET=y redo t.parallel


## Loop

This example demonstrates how lock-free approach allows to locate and avoid loop dependencies even in parallel builds.

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/loop
    redo parallel


