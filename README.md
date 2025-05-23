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

`redo` is incremental build systems engine. Any build project may be described as directed acyclic graph. The nodes of such graph are files (sources and targets) while branches indicate dependencies and are directed. The targets are built by the corresponding recipes. Recipe builds the target and describe the target's dependencies.

The build system to be driven by `redo` is the set of the recipes. `redo` implements an effective interface between the recipes, allowing to design reliable, flexible and easily extendable build systems.


## Build process quick overview

    redo [ target [ ... ] ] 

During the build process `redo` executes build recipes for targets with outdated dependencies. Recipes build targets and tell `redo` which files were used. Dependencies tracking is managed by `redo` and don't require any user's attention or intervention.


### Program and data flow `redo` -> recipe.

`redo` executes the build recipes passing 3 positional parameters:

$1 - target's name

$2 - target's family (explained below)

$3 - temporary file name to store the recipe output


### Program and data flow recipe -> `redo`.

Recipe stores the build result in $3 and register dependencies with

    depends-on [ dep [ ... ] ]

`redo` captures the recipe's exit code and in case of success replaces the target $1 with the temporary file $3, otherwise $3 is discarded.


### `redo` vs `depends-on`

`depends-on` is the link to `redo`. The main difference between `redo` and `depends-on` is that `depends-on` reports (if possible) about the targets built to the caller `redo` instance, while `redo` does not.


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


Dot-started recipes ( rules ) are able to build families of targets in their current dirs and all subdirs.


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

In case the appropriate recipe will be found, then it will be executed passing 3 described above positional parameters. The target family name ($2) will be derived from the recipe and target names:

    Recipe        Target ($1)     Family ($2)

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

* `-w` Treat loop dependencies as warnings and continue partial build. Handle with care and keep away from children. `REDO_WARNING={0,1}`

* `-e`, `-d` Enables building of recipes. `REDO_RECIPES={0,1}`

* `-f`, `-r` Log find_recipe() steps to stdout. `REDO_FIND={0,1}`

* `-t`, `-x` Tracing of non-executable recipes executing them with `/bin/sh -ex`. `REDO_TRACE={0,1}`

* `-l <log_name>` Log build process as Lua table. Requires log filename. Filename "1" redirects log to stdout, "2" to stderr.

* `-m <roadmap>` Build according to the [roadmap](samples/parallel#roadmap). If the requested roadmap file is not found then command-line arguments are used as targets. If the roadmap was imported successfully then command-line targets are ignored. Errors during the roadmap import lead to `exit(ERROR)`.

#### Tip

If log is written to stdout or stderr then recipes' output is enclosed properly and appears in the log as comments, keeping log as a valid Lua table. Debugging tastes better with

    redo -l 2 target 2>target.log

or

    redo -l 1 target >target.log 2>&1


## Implementation details

### Journals

Journals are created by `redo` for every successfully built target. The name of the journal file starts with `.do..` prefix followed by the target's filename. The journal consists of records. Each record describes certain dependency and contains its filename, ctime and hash of the content. If `x` target was built by the `x.do` recipe using `a`, `b` and `c` dependencies then journal `.do..x` will contain the records describing `x.do`, `a`, `b`, `c` and `x` files.

If some file is referenced by `depends-on` but have no recipe to be built, such file is source. Sources have no journals. 

The journals can not be targets, but can be used as sources.

#### WARNING!

Using files which names start with the journal prefix `.do..` may cause unexpected behaviour. Probably it is the good idea to not use such prefix for filenames in Your project's directories.


### More details of `redo` program flow

    redo xxx

1. `redo` tries to find the recipe able to build `xxx`.

2. If no recipe is found `redo` exits successfully.

3. If recipe is found then the next conditions are tested:

	3.1 Target's journal `.do..xxx` found.

	3.2 The most recent build of `xxx` was performed by the same recipe.

	3.3 All the target's dependencies are unchanged since the most recent build.

	3.4 The target `xxx` was not changed.

If all the described conditions are true, then refresh '.do..xxx' journal and exit successfully.

Worth mentioning that testing of the 3.3 condition is recursive. The steps described above - search for an appropriate recipe and testing the corresponding journal - are performed for all dependencies.

4. Execute the recipe and if it exits successfully, then write the result into `xxx`, else `redo` fails.

5. Update `.do..xxx` journal.


### Hashed sources aka self-targets

Targets are hashed once per build, while sources are hashed once per dependence. If Your project includes big source files required by more than one target, converting these sources into self-tagets will speed-up build and update.

Conversion can be provided with the help of the following simple recipe:

    if test -f "$1"; then  mv "$1" "$3"; fi

Adding to Your project `.do` file consisting of above shown command will convert all sources excepts active recipes to self-targets. Such conversion may slow-down projects with lot of small sources.


### Always out-of-date targets

Can be implemented using target's dependency on its own journal:

    depends-on .do..$1


### Recipes as targets

The current `redo` version follows approach of "do-layers". File belongs to the Nth do-layer if its name ends with N `.do` suffices. Targets belonging to the Nth do-layer can be built by (N+1)th do-layer recipes only. Technically it means that no trailing `.do` suffix can be stripped from the target's filename during the search for an appropriate recipe.


### Note about the soft links as targets

Recipes may create soft links as the targets. In case such target is linked to non-existing file then it will be hashed as non-existing (empty) file. And if the link will be removed redo will not be able to detect its disappearance and restore it.


### Loop dependencies

Are monitored unconditionally and issue error or warning if found.


### Parallel builds

The technique for parallel builds implementation in recipes is described in [samples/parallel](samples/parallel).


### Passes and retries

The current version of `redo` is lock-free. The list of the target names is passed across trying to build each. Any target's build failure cause immediate exit returning `ERROR` (1). If target is busy, move to the next target. The pass is successful if at least one of the targets was built successfully. After the successful pass the next pass (if necessary) is started immediately. Otherwise (all targets are busy) the retry pass is started after some delay. This delay is doubled after retry and reset after successful pass. After the certain number of an unsuccessful passes `redo` exits returning `BUSY` (EX_TEMPFAIL defined in `<sysexits.h>`).


### `redo` retry delays.

`REDO_RETRIES` environment variable defines the number of consequent unsuccessful passes allowed for `redo` before exiting as `BUSY`. For `redo` default `REDO_RETRIES` value is `RETRIES_DEFAULT` (defined in redo.c). For `depends-on` default `REDO_RETRIES` value is 0, meaning the single pass. `REDO_RETRIES` is being unset after getting its value, so it is not inherited by the child processes.

Constants `SHORTEST` and `SCALEUPS` defined in redo.c determine the duration of the delays between the retries. After each unsuccessful pass the random delay in the range [x .. 2 * x] is inserted, where x is reset to SHORTEST msec after each successful pass (and at the startup) and is doubled after each consequent retry but no more than SCALEUPS times.  

As an example

    #define SHORTEST 10
    #define SCALEUPS 6

gives minimal dalay in the [10 .. 20] msec range (15 msec mean time) and the maximum delay in the range [640 .. 1280] msec (0.96 sec mean time).

    #define RETRIES_DEFAULT 10

gives 4 sec total wait mean time.


### Hints

You can test which recipes will be encountered by `redo` appropriate to build certain `<target>` with

    redo -f <target>


### Tricks

The sequence `.do.` found inside the supposed target's name during the find_recipe() search for appropriate recipe will interrupt the search routine. That's why it is not recommended for plain builds. But it may be used with care for the targets, which need cwd-only recipe search or must escape the omnivorous `.do` visibility area.

Searching in cwd only:

    $ redo -f cwd.do.only
    --[[
    cwd.do.only.do
    --]]

Searching in cwd and updirs:

    $ redo -f cwd.and.updirs.do.too
    --[[
    cwd.and.updirs.do.too.do
    .and.updirs.do.too.do
    .updirs.do.too.do
    ../.and.updirs.do.too.do
    ../.updirs.do.too.do
    --]]

Making file invisible for all recipes incuding `.do`:

    $ redo -f .do.not.build
    --[[
    --]]


### Troubleshooting

If the build was interrupted then some locks may remain uncleared. Such locks can be located with the help of the build log. See [samples/locks](samples/locks).


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>


