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




## Appendix

dev4 is an experimental still full-functional branch. Its purpose is to lower the number of reserved words used by `redo`. `default` first filename is now regular word and have no special meaning. Dotdofiles play the role of default dofiles. Such approach is expected to be more consistent.


### Motivation

Improve performance avoiding unnecessary targets scripts' execution. As deep as possible dive inside the dependencies tree and attempt to execute scripts placed deeper prior to those placed closer to the build root. Let's imagine, that `A` depends on `B`, and `B` depends on `C` (some source). Then `update_dep()` recurses down to `C`, checks its hash, and in case it was changed, runs `B.do`. Then `B` hash is checked and `A.do` is run then and only if `B`'s hash differs from its previous value, stored in the `..do..A` file.

Implement lock-free loop dependencies detection, allowing safe `redo` parallelizing.

The current implementation (dev4 branch) follows D.J.Bernstein's guidelines on distinguishing sources and targets. Targets do have corresponding `.do` files, while sources - don't. KISS.

Actively used nowadays implementations:

https://github.com/apenwarr/redo

https://github.com/leahneukirchen/redo-c

http://www.goredo.cypherpunks.ru

use more sophisticated though more complicated and not so clear logic. The current `redo` version is an attempt to make it fully controllable with sources and `.do` files, which means, that `redo` internals are pure derivatives of sources and `.do` files and don't require any user attention or interventions.


### Test-drive

    . ./redo.do


### Kick-start

    . ./redo.do $HOME/.local/bin


### Standalone build

Add `redo.c` and `redo.do` files to Your project and build with

    (. ./redo.do; redo <target>)


### Compatibility notes

#### Major

`default` prefix has no special meaning in conjunction with `.do` suffix. Sequence `default` can be used without any limitations in sources', targets' and recipes' names and follows common dofiles' search rules.

Dotdofiles (like `.o.do`, `.x.do.do`, `.do`) are able to build groups of files with corresponding extensions:

* `.o.do` builds all `*.o` files

* `.x.do.do` builds all `*.x.do` files

* `.do` builds all `*` files

#### Important

Non-existing targets are not expected out-of-date unconditionally. If for example `foo.do` script produces no output and exits successfully, then record about an empty (non-existing) file `foo` is written into the corresponding `..do..foo` file and target `foo` is expected up-to-date until it become existing and not empty (non-existent and empty targets have the same hashes). Such behaviour eliminates the need for `redo-ifcreate` and allows to avoid enforcement to produce zero-sized files. Of course, You can use them if it fits Your taste and notion.

#### Less important

stdout of `*.do` scripts is not captured. Feel free to start Your recipes with

    exec > $3 

The `redo` binary itself never create or delete directories. Let dofiles do this job.

#### Unimportant

No default target.

`redo` forces rebuild of up-to-date targets only being told `-f`.

#### Implementation specific

Sources with the names `..do..*` may be used for special purposes only - see "Redo-always" section.

Sequence `..do.` inside the target name has special purpose (see "Tricks" section) and is not recommended for use somewhere inside the filenames.


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

* `-w` Log find_dofile() steps to stdout. `REDO_WHICH_DO={0,1}`

* `-d depth` limit of dependency tree nodes full names to be printed.


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


### Redo-always

In fact current version implements only 2 utilities from redo family: `redo-ifchange` and `redo-always`. This short list may be reduced to `redo-ifchange` only. `redo-always` may be easily implemented as

    redo ..do..$1

using the fact, that `..do.*` files are not allowed to have `*.do` files and can not be targets. This approach is prefered over plain old "redo-always", because give some additional abilities, see "Hints" section.

In other words `redo-ifchange`, `redo-icreate` and `redo-always` links are redundant, everything may be done with `redo` itself.


### Dotdofiles

Dotdofiles do the job which `default*.do` files do in majority of `redo` implementations.

    $ redo -w x.y
    >>>> /tmp/x.y
    x.y.do
    .y.do
    .do
    ../.y.do
    ../.do

Compatibility of traditional `default*.do` files with the current version can be provided with the help of copying them with `default` replaced with `''`:

    cp default.o.do .o.do
    cp default.bin.do .bin.do
    cp default.do .do


### Doing dofiles

`.do` filename extension has special meaning for `redo`. At the first glance it divides all files into two categories - ordinary files and dofiles. But what about doing dofiles? Current version follows approach of "do-layers". Ordinary files (lacking `.do` filename extension) belongs to 0th do-layer. `*.do` files belong to the 1st do-layer. `*.do.do` files - to the 2nd do-layer and so forth.

The rule of doing dofiles is that file belonging to the Nth do-layer can be done by (N+1)th do-layer file only. Technically it means that any trailing `.do` suffix will not be excluded from the filename during the search for an appropriate dofile.


### "Imaginary" target

The current version of redo supports the nameless targets such as :

    redo ''

    redo ./

    redo some-dir/

Of course such targets can not exist, but they may have the corresponding script, and its name is `.do`.

Please keep in mind that `.do` in the current version is reincarnation of `default.do` and attempts to do everything, and must contain the recipe for converting pure sources into self-targets:

    test -f "$1" && mv "$1" "$3"

If You prefer makefile-like `.do`, probably You use `case` selector for distinguishing the recipes. Then the default branch recipe may look like

    *) test -f "$1" && mv "$1" "$3" ;;


### Troubleshooting

If for some reason Your build was interrupted and You suffer of fake "Target busy" messages, then some locks remain uncleared. You can remove them with the help of:

    redo -ui ''

keeping in mind that use of this option is safe only if possibility of parallel build is absolutely obviated. 


### Hints

If You implement `redo-always` as `redo ..do..$1` then You can obtain the list of redone-always targets with:

    redo -os '' | sed -n 's/\.\.do\.\.//p' | sort | uniq

for the project already built.


Test Your project for warnings without touching targets and refreshing dependency records

    redo -ul ''


### Tricks

As it was noted above the sequence `..do.` has special meaning. If it is found inside the supposed target's name during the search for appropriate dofile, it interrupts the search routine. That's why it is not recommended for plain builds. But it may be used for targets, which need cwd-only dofile search or must avoid doing by omnivorous `.do`.

Searching in cwd and updirs, involves `.do`:

    $ redo -w x.cwd-n-upper
    >>>> /tmp/x.cwd-n-upper
    x.cwd-n-upper.do
    .cwd-n-upper.do
    .do
    ../.cwd-n-upper.do
    ../.do

Searching in cwd only, `.do` rests:

    $ redo -w x.cwd-only..do.
    >>>> /tmp/x.cwd-only..do.
    x.cwd-only..do..do
    .cwd-only..do..do


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

