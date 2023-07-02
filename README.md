# redo-c

redo-c is an implementation of the [redo build system](http://cr.yp.to/redo.html) (designed by Daniel J. Bernstein) in portable C with zero external dependencies.


## Documentation

Please refer to the documentation for
[redo in Python](https://github.com/apenwarr/redo/blob/master/README.md),
or the [tutorial by Jonathan de Boyne Pollard](http://jdebp.info/FGA/introduction-to-redo.html)
for usage instructions.


## Copying

To the extent possible under law, Leah Neukirchen <leah@vuxu.org>
has waived all copyright and related or neighboring rights to this work.

http://creativecommons.org/publicdomain/zero/1.0/


## General description

`redo` is incremental build systems engine. Build system to be driven by `redo` is the set of the recipes. `redo` implements an effective interface between the recipes, allowing to design reliable, flexible and easily extendable build systems.

The targets and sources of such build tree may be interpreted as nodes of some directed acyclic graph (DAG), while recipes contain the branches descriptions.


## Build process quick overview

    redo [ target [ ... ] ] 

During the build process `redo` executes build recipes for targets with outdated dependencies. Recipes build targets and tell `redo` which files were used. Dependencies tracking is managed by `redo` and don't require any user's attention or intervention.


### Program and data flow `redo` -> recipe.

`redo` executes the build recipes passing 3 positional parameters:

$1 - target's name

$2 - target's class (explained below)

$3 - temporary file name to store the recipe output


### Program and data flow recipe -> `redo`.

Recipe stores the build result in $3 and register dependencies with

    depends-on [ dep [ ... ] ]

`redo` captures the recipe exit code and in case of success replaces the target $1 with the temporary file $3, otherwise $3 is discarded.


## The magic of `redo`

In fact `depends-on` in the recipe envokes another instance of `redo` and the new instance communicate with the recipe caller `redo` "behind the closed doors". And if the requested dependency has its own build recipe, the process will recurse deeper independently of the current recipe execution.

So the structure of the build tree and the target build steps are described per-node and can be easily modified, extended, truncated and any part of the tree may be easily reused by another build project using any node as an entry point.


## More details about recipes

When `redo` is called to build some target, the first task to accomplish is to find the recipe able to build the requested target.

Only files having `.do` suffix are identified as recipes by `redo`.

    Recipe name    Targets to build

    x.do           x

    a.b.c.do       a.b.c

    .b.do          *.b, */*.b, */*/*.b, ...

    .b.c.do        *.b.c, */*.b.c, */*/*.b.c, ...


Dot-started recipes are able to build classes of targets in their current dirs and all subdirs.


### Selecting an appropriate recipe.

    redo x.y.z

1. The search for recipe will start with `x.y.z.do` in the `x.y.z` directory.

2. First name and all extensions (excepts trailing `.do` ones) of supposed recipe filename will be sequentially stripped in the order and recipes

*   `.y.z.do`

*   `.z.do`

*   `.do`

will be looked for in the `x.y.z` directory

3. The same candidates as in step 2 will be looked for in all up-dirs.

If no recipe able to build the requested target will be found, then `redo` exits successfully doing nothing.

In case the appropriate recipe will be found, then it will be executed passing 3 described above positional parameters. The target class name ($2) will be derived from the recipe and target names:

    Recipe        Target ($1)     Class ($2)

    a.b.c.do      a.b.c           (empty)

    .d.e.do       a.b.c.d.e       a.b.c

    .d.e.do       x.d.e           x

    .do           x.y.z           x.y.z

Recipe is always executed by `redo` in the recipe's directory. If target is located in subdir, then all 3 parameters will be relative paths.

Recipe may be executable - binary or some script starting with the proper shebang. Such recipes are simply executed. If recipe is not executable it is considered shell script and is executed with `/bin/sh -e`.


## Usage

### Standalone build

Add `redo.c` and `redo.do` files to Your project and build with

    (. ./redo.do; redo <target>)


### Test-drive

    . ./redo.do

will build the `redo` binary, create `depends-on` link and adjust the PATH variable to allow single-session use. 


### Kick-start

    . ./redo.do $HOME/.local/bin

will build the `redo` binary, create `depends-on` link and copy them to the already present in the PATH preferred directory.


### Build `redo` with `redo`

    redo redo


### Options available

* `-x` Non-executable recipes will be executed with `/bin/sh -ex`. `REDO_TRACE={0,1}`

* `-d` Enables building of recipes. `REDO_DOFILES={0,1}`

* `-w` Treat loop dependencies as warnings and continue partial build. Handle with care and keep away from children. `REDO_WARNING={0,1}`

* `-f` Log find_dofile() steps to stdout. `REDO_FIND={0,1}`

* `-l <log_name>` Log build process as Lua table. Requires log filename. Filename "1" redirects log to stdout, "2" to stderr.


## Implementation details

### Prerequisites

Prerequisites are created by `redo` for every successfully built target. They consist of records. Each record describes certain dependency and contains its filename, ctime and hash of the content. If `x` target was built by the `x.do` recipe using `a`, `b` and `c` dependencies then prerequisites file `.do..x` will contain the records describing `x.do`, `a`, `b`, `c` and `x` files.

If some file is referenced by `depends-on` but have no recipe to be built, such file is source. Sources have no prerequisites. 

Prerequisites can not be targets, but can be used as sources.


### More details of `redo` program flow

    redo xxx

1. `redo` tries to find the recipe able to build `xxx`.

2. If no recipe is found `redo` exits successfully.

3. If recipe is found then the next conditions are tested:

	3.1 Target's prerequisites `.do..xxx` found.

	3.2 The most recent build of `xxx` was performed by the same recipe.

	3.3 All the target's dependencies are unchanged since the most recent build.

	3.4 The target `xxx` was not changed.

If all the described conditions are true, then refresh '.do..xxx' prerquisites and exit successfully.

Worth mentioning that testing of the 3.3 condition is recursive. The steps described above - search for an appropriate recipe and testing the corresponding prerequisites - are performed for all dependencies.

4. Execute the recipe and if it exits successfully, then write the result into `xxx`, else `redo` fails.

5. Update `.do..xxx` prerequisites.


### Hashed sources aka self-targets or semi-targets.

Targets are hashed once per build, while sources are hashed once per dependence. If Your project includes big source files required by more than one target, converting these sources into self-tagets will speed-up build and update.

Conversion can be provided with the help of the following simple recipe:

    test -f $1 && mv $1 $3

Adding to Your project `.do` file consisting of above shown command will convert all sources excepts active recipes to self-targets. Such conversion may slow-down projects with lot of small sources.


### Loop dependencies

Are monitored unconditionally and issue error or warning if found.


### Parallel builds

The technique for parallel builds implementation in recipes is described in `samples/parallel` examples.


### Always out-of-date targets

Can be implemented using target's dependency on its own prerequisites:

    depends-on .do..$1


### Recipes as targets

The current `redo` version follows approach of "do-layers". File belongs to the Nth do-layer if its name ends with N `.do` suffices. Targets belonging to the Nth do-layer can be built by (N+1)th do-layer recipes only. Technically it means that no trailing `.do` suffix can be stripped from the target's filename during the search for an appropriate recipe.


### Hints

You can test which dofiles will be encountered by `redo` appropriate to build certain <target> with

    redo -f <target>


### Tricks

The sequence `.do.` found inside the supposed target's name during the find_dofile() search for appropriate recipe will interrupt the search routine. That's why it is not recommended for plain builds. But it may be used with care for the targets, which need cwd-only recipe search or must escape the omnivorous `.do` visibility area.

Searching in cwd only:

    $ redo -l 1 cwd.do.only
    return {
      "/tmp/cwd.do.only",
    --[[
    cwd.do.only.do
    --]]
    }

Searching in cwd and updirs:

    $ redo -l 1 cwd.and.updirs.do.too
    return {
      "/tmp/cwd.and.updirs.do.too",
    --[[
    cwd.and.updirs.do.too.do
    .and.updirs.do.too.do
    .updirs.do.too.do
    ../.and.updirs.do.too.do
    ../.updirs.do.too.do
    --]]
    }


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>


