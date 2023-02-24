# redo-c

redo-c is an implementation of the redo build system (designed by
Daniel J. Bernstein) in portable C with zero external dependencies.

## Documentation

Please refer to the documentation for
[redo in Python](https://github.com/apenwarr/redo/blob/master/README.md),
or the [tutorial by Jonathan de Boyne Pollard](http://jdebp.info/FGA/introduction-to-redo.html)
for usage instructions.

## Notes about the redo-c implementation of redo

* Without arguments, `redo` behaves like `redo all`.

* `.do` files always are executed in their directory, arguments are
  relative paths.

* Standard output of `.do` files is only captured as build product if
  `redo -s` is used, or the environment variable `REDO_STDOUT` is set to 1.
  Else, standard output is simply displayed.

* Non-executable `.do` files are run with `/bin/sh -e`.
  `redo -x` can be utilized to use `/bin/sh -e -x` instead, for
  debugging `.do` files or verbose builds.

* Executable `.do` files are simply executed, and should have a shebang line.

* When a target makes no output, no target file is created.  The target
  is considered always out of date.

* `default.do` files are checked in all parent directories up to `/`.

* Parallel builds can be started with `redo -j N` (or `JOBS=N redo`),
  this uses a job broker similar to but not compatible with GNU make.

* To detect whether a file has changed, we first compare `ctime` and
  in case it differs, a SHA2 hash of the contents.

* Dependencies are tracked in `.dep.BASENAME` files all over the tree.
  This is an implementation detail.

* Builds can be started from every directory and should yield same results.

* `redo -f` will consider all targets outdated and force a rebuild.

* `redo -k` will keep going if a target failed to build.

## Copying

To the extent possible under law, Leah Neukirchen <leah@vuxu.org>
has waived all copyright and related or neighboring rights to this work.

http://creativecommons.org/publicdomain/zero/1.0/




## Redo as distributed computations engine

### Terms and definitions

#### Variables

Variables are files.

#### Functions

Functions are executable files or the shell scripts. Their names must contain the `.do` suffix. Functions may have input parameters - variables. Input parameters can be declared inside the function with the help of `depends-on` call. The result of function evaluation is one variable. Function's result must be unambiguous and fully determined by its input parameters.

##### Selecting an appropriate function.

For example `redo` is asked to build the variable `x.y.z`:

    redo x.y.z

1. The search will start with `x.y.z.do` in the `x.y.z` directory.

2. First name and all extensions (excepts trailing `.do` ones) will be sequentially stripped in the order and files

    .y.z.do

    .z.do

    .do

will be looked for in the `x.y.z` directory

3. The same candidates as in step 2 will be looked for in all up-dirs.

In case the appropriate function will be found, then class name will be derived from the initial variable's name extended with `.do` suffix and actually chosen function's basename, as complement of actual choice to initial fullname.

    Variable    Function    Class

    x.y.z       .y.z.do     x

    a.b.c       .c.do       a.b

    q.w.e       .do         q.w.e

    a.s.d       a.s.d.do    ''

##### Functions' invocation.

`redo` invokes the `<function>` chosen for `<variable>` as:

    <function> <variable> <class> <tmpfile>

where `<tmpfile>` is proposed for `<function>` as intermediate result storage and will replace `<variable>` if `<function>` exits successfully.

`<function>` is executed in its directory, `<variable>`, `<class>` and `<tmpfile>` are passed as relative paths.

#### Prerequisites

Prerequisites are variables, created by `redo` per every target. They  consist of records. Each record describes certain variable and contains variable's filename, its ctime and hash of variable's content.

Prerequisites of `some-var` variable are stored in `.do..some-var` file in the same directory with `some-var`.

If variable `x` was computed by function `f` using input parameters `a`, `b` and `c`:

    x = f(a, b, c)

then prerequisites variable named `.do..x` will contain the records about `f`, `a`, `b`, `c` and `x` variables.

##### Targets

Variables computed by some function are named targets. Targets have prerequisites.

##### Sources

If no function can be found able to compute the variable, such a variable is source. Sources have no prerequisites.

Prerequisites are to be managed by `redo` only. Thay can not be targets, but can be sources.


### Usage

If we want to get `xxx` computed we call

    redo xxx

1. `redo` tries to find the function, able to compute `xxx`.

2. If no function is found, then prerequisites `.do..xxx` are removed (if existing) and `redo` exits.

3. Prerequisites studying. Test the next conditions:

	3.1 No prerequisites found.

	3.2 The most recent computation of `xxx` was performed by another function.

	3.3 Any of the input parameters of the function changed.

	3.4 The variable `xxx` was changed.

If any of the described conditions is true, then goto 4, otherwise goto 5.

Worth mentioning that testing of the 3.3 condition is recursive. The steps described above - search for an appropriate function and testing the corresponding prerequisites - are performed for all of the input parameters.

4. Execute the function and if it exits successfully, then write the result into `xxx`.

5. Update `.do..xxx` prerequisites.

6. Done.


### Test-drive

    . ./redo.do


### Kick-start

    . ./redo.do $HOME/.local/bin


### Standalone build

Add `redo.c` and `redo.do` files to Your project and build with

    (. ./redo.do; redo <target>)


### Options available

#### Build options

* `-f` All targets are considered outdated. Usefulness doubtful. `REDO_FORCE={0,1}`

* `-x` See above. `REDO_TRACE={0,1}`

* `-e`, `-ee`. Enables doing of .do files. `REDO_DOFILES={0,1,2}`. 0 (default) suppress doing of dofiles, 1 (`-e`) suppress doing of dotdofiles, 2 (`-ee`) allows to do anything.

* `-i` Ignore locks - be watchful and handle with care. Use only if You are absolutely sure, that no parallel builds will collide - results unpredictable. `REDO_IGNORE_LOCKS={0,1}`

* `-l` Treat loop dependencies as warnings and continue partial build. Handle with care and keep away from children. `REDO_LOOP_WARN={0,1}`


#### Diagnostic output options

* `-n` Inhibits `*.do` files execution. Supersedes `-f`. Suppresses dependency files' refreshing.

* `-u` "up-to-date" imitation. Implies `-n`. Project dependency tree is walked through as if all dependencies are up-to-date. Implicit `-n` means that only the branches already built can be scanned.

* `-s` List source files' full paths to stdout. `REDO_LIST_SOURCES={0,1,2}`

* `-t` List target files' full paths to stdout. `REDO_LIST_TARGETS={0,1,2}`

* `-o` "outdated" modifier for `-st` options. Implies `-u`.

* `-w` Log find_dofile() steps to stdout. Have no effect in `-u` and `-o` modes. `REDO_WHICH_DO={0,1}`

* `-d depth` of the nodes to be displayed. `depth` equal to 0 means "display all". Positive `depth` means "equal to". Negative `depth` means "less or equal".


### Hashed sources aka self-targets or semi-targets.

Output options `-st` can be combined in order to achieve desired output:

* `-s` sources only

* `-st` self-targets only

* `-sst` sources and self-targets

* `-t` full targets only

* `-stt` all targets

* `-sstt` all files

Targets are hashed once per build, while sources are hashed once per dependence. If Your project includes big source files required by more than one target, converting these sources into self-tagets will speed-up build and update.

Conversion can be provided with the help of the following simple dofile:

    test -f $1 && mv $1 $3

Adding to Your project `.do` file consisting of above shown command will convert all sources excepts active dofiles to self-targets. Such conversion may slow-down projects with lot of small sources.


### Loop dependencies

Are monitored unconditionally and issue error or warning if found.


### Parallel builds

Can be implemented in cooperative form. See `samples/parallel`


### Always out-of-date targets

Can be achieved with the help of

    depends-on .do..$1


### Doing dofiles

`.do` filename extension has special meaning for `redo`. At the first glance it divides all files into two categories - ordinary files and dofiles. But what about doing dofiles? The current version follows approach of "do-layers". Ordinary files (lacking `.do` filename extension) belongs to 0th do-layer. `*.do` files belong to the 1st do-layer. `*.do.do` files - to the 2nd do-layer and so forth.

The rule of doing dofiles is that file belonging to the Nth do-layer can be done by (N+1)th do-layer file only. Technically it means that any trailing `.do` suffix will not be excluded from the filename during the search for an appropriate dofile.


### Troubleshooting

If for some reason Your build was interrupted and You suffer of fake "Target busy" messages, then some locks remain uncleared. You can remove them with the help of:

    redo -ui <target>

keeping in mind that use of this option is safe only if possibility of parallel build is absolutely obviated. 


### Hints

You can obtain the list of always out-of-date targets with:

    redo -os <target> | sed -n 's/\.do\.\.//p' | sort | uniq

for the project already built.


Test Your project for warnings without touching targets and refreshing dependency records

    redo -ul <target>


### Tricks

The sequence `.do.` inside the variable's filename has special meaning. If it is found inside the supposed target's name during the search for appropriate dofile, it interrupts the search routine. That's why it is not recommended for plain builds. But it may be used for targets, which need cwd-only dofile search or must escape the omnivorous `.do` visibility area.

Searching in cwd only:

    $ redo -w cwd-only.do.
    >>>> /tmp/cwd-only.do.
    cwd-only.do..do

Searching in cwd and updirs:

    $ redo -w cwd.and.updirs.do.
    >>>> /tmp/cwd.and.updirs.do.
    cwd.and.updirs.do..do
    .and.updirs.do..do
    .updirs.do..do
    ../.and.updirs.do..do
    ../.updirs.do..do




Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

