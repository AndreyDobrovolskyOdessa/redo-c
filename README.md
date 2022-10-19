# redo-c

redo-c is an implementation of the redo build system (designed by
Daniel J. Bernstein) in portable C with zero external dependencies.

## Documentation

Please refer to the documentation for
[redo in Python](https://github.com/apenwarr/redo/blob/master/README.md),
or the [tutorial by Jonathan de Boyne Pollard](http://jdebp.eu/FGA/introduction-to-redo.html)
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

Variables are sequences of bytes. Implemented as files, placed in certain directories of the hierarchical filesystem.

#### Functions

Functions are executable variables. Functions are stored in the files which names are ended with `.do` suffix. Functions may have input parameters - variables. The result of function evaluation is one variable.

#### Prerequisites

Prerequisites are variables, created by `redo` at the moment of another variable's computation by some function. Prerequisites themselves can not be an immediate result of any function.

##### Records

Records are components of prerequisites. The record describes certain variable. Each record must contain variable's filename, its ctime and hash of variable's content.

Variable's record is stored in prerequisites when variable is used. Records are managed by `redo`.


Prerequisites of "some-var" variable are stored in ".do..some-var" file in the same directory with "some-var".

If variable "x" was computed by function "f" using input parameters "a", "b" and "c":

    x = f(a, b, c)

then ".do..x" file will contain records about "f", "a", "b", "c" and "x" variables. "f" and "x" records are written by `redo`, while "a", "b" and "c" records must be created by "f" using `depends-on` call:

    depends-on a b c

##### Targets

Variables, having prerequisites, are named targets.

##### Sources

Variables, having no prerequisistes, are named sources.



### Usage

If we want to get "xxx" computed we call

    redo xxx

1. `redo` tries to find function, able to compute "xxx".

2. If no such function found, prerequisites for "xxx" are checked and removed if existing.

3. If function "xxx.do" is found appropriate for building "xxx", then start

4. Prerequisites studying.

	4.1 If no prerequisites found, then goto 5 

	4.2 If the most recent computation of "xxx" was provided by another function or "xxx.do" was changed after the latest build, then goto 5

	4.3 If any of the input parameters of "xxx.do" changed (imply `redo <parameter>`), then goto 5

	4.4 If requested variable "xxx" was changed, then goto 5

	4.5 goto 6

5. Execute "xxx.do" and write the result into "xxx".

6. Update ".do..xxx" prerequisites.



#### Choosing appropriate function.

An applicability of the function to the certain variable is detected according to variables' and functions' names and locations in filesystem directories.

For example `redo` was asked to build variable "x.y.z":

    redo x.y.z

1. The search is started with "x.y.z.do" in the "x.y.z" directory.

2. First name and all extensions are sequentially stripped in the order and files

    .y.z.do
    .z.do
    .do

are looked for in the "x.y.z" directory

3. The same candidates as in step 2 are looked for in all up-dirs.

In case the appropriate function was found, class name is derived from the initial variable's name extended with ".do" suffix and actual choice, as complement of actual choice to fullname.

Variable	Function	Class

x.y.z		.y.z.do		x

a.b.c		.c.do		a.b

q.w.e		.do		q.w.e

a.s.d		a.s.d.do	<empty>


#### Functions' invocation.

`redo` invokes the <function> chosen for <variable> as:

    <function> <variable> <class> <tmpfile>

where <tmpfile> is proposed for <function> as intermediate storage and will replace <variable> if <function> exits successfully.



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

    redo -ui ''

keeping in mind that use of this option is safe only if possibility of parallel build is absolutely obviated. 


### Hints

You can obtain the list of always out-of-date targets with:

    redo -os '' | sed -n 's/\.do\.\.//p' | sort | uniq

for the project already built.


Test Your project for warnings without touching targets and refreshing dependency records

    redo -ul ''


### Tricks

As it was noted above the sequence `.do.` has special meaning. If it is found inside the supposed target's name during the search for appropriate dofile, it interrupts the search routine. That's why it is not recommended for plain builds. But it may be used for targets, which need cwd-only dofile search or must avoid doing by omnivorous `.do`.

Searching in cwd and updirs, involves `.do`:

    $ redo -w x.cwd-n-upper
    >>>> /tmp/x.cwd-n-upper
    x.cwd-n-upper.do
    .cwd-n-upper.do
    .do
    ../.cwd-n-upper.do
    ../.do

Searching in cwd only, `.do` rests:

    $ redo -w x.cwd-only.do.
    >>>> /tmp/x.cwd-only.do.
    x.cwd-only.do..do
    .cwd-only.do..do


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

