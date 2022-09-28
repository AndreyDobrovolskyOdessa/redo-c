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

dev3 branch attempts to minimize hashing expenses.  


### Motivation

Improve performance avoiding unnecessary targets scripts' execution. As deep as possible dive inside the dependencies tree and attempt to execute scripts placed deeper prior to those placed closer to the build root. Let's imagine, that `A` depends on `B`, and `B` depends on `C` (some source). Then `update_dep()` recurses down to `C`, checks its hash, and in case it was changed, runs `B.do`. Then `B` hash is checked and `A.do` is run then and only if `B`'s hash differs from its previous value, stored in the `.redo.A` file.

The current implementation (dev3 branch) follows D.J.Bernstein's guidelines on distinguishing sources and targets. Targets do have corresponding `.do` files, while sources - don't. KISS.

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

#### Important

Non-existing targets are not expected out-of-date unconditionally. If for example `foo.do` script produces no output and exits successfully, then record about an empty (non-existing) file `foo` is written into the corresponding `.redo.foo` file and target `foo` is expected up-to-date until it become existing and not empty (non-existent and empty targets have the same hashes). Such behaviour eliminates the need for `redo-ifcreate` and allows to avoid enforcement to produce zero-sized files. Of course, You can use them if it fits Your taste and notion.


`.redo.` prefix is reserved for redo purposes. Files named `.redo.*` shouldn't be used in the current version builds as targets or recipes, but for some special purpose may be sources - see "Redo-always" section.


#### Less important

stdout of `.do` scripts is not captured. Feel free to start Your recipes with

    exec > $3 


The 'redo' binary itself never create or delete directories. Let dofiles do this job.


#### Unimportant

No default target.

`redo` force rebuild of up-to-date targets only being told `-f`.


### Options available

* `-f` All targets are considered outdated. Usefulness doubtful. `REDO_FORCE={0,1}`

* `-n` Inhibits `.do` files execution. Supersedes `-f`. Suppress dependency files' refreshing.

* `-x` See above. `REDO_TRACE={0,1}`

* `-s` List source files' full paths to stdout. `REDO_LIST_SOURCES={0,1,2}`

* `-t` List target files' full paths to stdout. `REDO_LIST_TARGETS={0,1,2}`

* `-o` "outdated" modifier for `-st` options. Implies `-n`. Project dependency tree is walked through as if all dependencies are up-to-date but outputing the outdated and selected ones' names. Implicit `-n` means that omly the branches already built are scanned.

* `-w` Log find_dofile() steps to stdout. If dependency tree includes redone-always targets this option may taste better with `-o` modifier.

* `-i` Ignore locks - be watchful and handle with care. Use only if You are absolutely sure, that no parallel builds will collide - results unpredictable. `REDO_IGNORE_LOCKS={0,1}`

* `-e`, `-ee`. Enables doing of .do files. `REDO_DOFILES={0,1,2}`. 0 (default) suppress doing of `*.do` files, 1 (`-e`) suppress doing of `default*.do` files, 2 (`-ee`) allows to do anything.

* `-l` Treat loop dependencies as warnings and continue partial build. Handle with care and keep away from children.


### Semi-targets

Semi-targets are the targets without dependencies (hashed sources). Output options `-st` can be combined in order to achieve desired output:

* `-s` sources only

* `-st` semi-targets only

* `-sst` sources and semi-targets

* `-t` full targets only

* `-stt` all targets

* `-sstt` all files

Targets are hashed once per build, while sources ae hashed once per dependence. If Your project includes big source files required by more than one target, converting this sources into semi-tagets will speed-up build and update.

Conversion can be provided with the help of the following simple dofile:

    test -f $1 && mv $1 $3

Adding to Your project `default.do` file consisting of above shown command will convert all sources excepts active dofiles to semi-targets. Such conversion may slow-down projects with lot of small sources.

If You prefer makefile-like `default.do`, probably You use `case` selector for distinguishing the recipes. Then the default branch recipe may look like

    *) test -f $1 && mv $1 $3 ;;


### Loop dependencies

Are monitored unconditionally and issue error or warning if found.


### Redo-always

In fact current version implements only 2 utilities from redo family: `redo-ifchange` and `redo-always`. This short list may be reduced to `redo-ifchange` only. `redo-always` may be easily implemented as

    redo .redo.$1

using the fact, that `.redo.*` files are not allowed to have `*.do` files and can not be targets. This approach is prefered over plain old "redo-always", because give some additional abilities, see "Hints" section.

In other words `redo-ifchange`, `redo-icreate` and `redo-always` links are redundant, everything may be done with `redo` itself.


### "Imaginary" target

The current version of redo supports the nameless targets such as :

    redo ''

    redo ./

    redo some-dir/

Of course such targets can not exist, but they may have the corresponding script, named `.do`. If You want to build `''` target, then `.do` script will be looked for in the target directory and all upper dirs, without any `default` prefixes applicable. Such "imaginary" target may be an interesting replace for `all` target. The difference is in `default` rules, which are applicable for `all` target, but are ignored for `''` target.


### Troubleshooting

If for some reason Your build was interrupted and You suffer of fake "Target busy" messages, then some locks remain uncleared. You can remove them with the help of:

    redo -i ''

keeping in mind that use of this option is safe only if possibility of parallel build is absolutely obviated. 


### Hints

If You implement `redo-always` as `redo .redo.$1` then You can obtain the list of redone-always targets with:

    redo -os '' | sed -n 's/\.redo\.//p' | sort | uniq

for the project already built.


Test Your project for warnings without touching targets and refreshing dependency records

    redo -ol ''



Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

