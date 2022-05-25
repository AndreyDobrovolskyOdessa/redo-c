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

### Motivation

Improve performance avoiding unnecessary targets scripts' execution. As deep as possible dive inside the dependencies tree and attempt to execute scripts placed deeper prior to those placed closer to the build root. Let's imagine, that A depends on B, and B depends on C (some source). Then update_target() recurses down to C, checks its hash, and in case it was changed, runs B.do. Then B hash is checked and A.do is run then and only if B's hash differs from its previous value, stored in the .dep file.

### Test-drive

    DESTDIR= ; . ./redo.do

### Kick-start

    (DESTDIR=$HOME/.local/bin ; . ./redo.do)


### Compatibility notes

#### Important

The current implementation (dev2 branch) follows D.J.Bernstein's guidelines on distinguishing sources and targets. Targets do have corresponding .do files, while sources - don't. KISS.

Actively used nowadays implementations:

https://github.com/apenwarr/redo

https://github.com/leahneukirchen/redo-c

http://www.goredo.cypherpunks.ru

use more sophisticated though more complicated and not so clear logic, which depends not only on .do files configuration, but on the internal state of the build system too.


Non-existing targets are not expected out-of-date unconditionally. If .do script produces no output and exits successfully, then record about an empty (non-existing) file is written into the corresponding .dep file and target is expected up-to-date until it become existing and not empty (non-existent and empty targets have the same hashes). Such behaviour eliminates the need for "redo-ifcreate" and allows to avoid enforcement to produce zero-sized files. Of course, You can use them if it fits Your taste and notion.


#### Less important

stdout of .do scripts is not captured. Feel free to start Your recipes with

    exec > $3 


#### Unimportant

No default target.

"redo" doesn't force execution of up-to-date targets' dofiles.


#### Hints

If You prefer makefile-like default.do, probably You use "case" selector for distinguishing the recipes. Then the default branch recipe may look like

    *) test -e $1 && mv $1 $3 ;;

In fact current version implements only 2 utilities from redo family: redo-ifchange and redo-always. This short list may be reduced to redo-ifchange only. redo-always may be easily implemented as

    redo-ifchange .dep.$1

using the fact, that dot-files are invisible for default*.do files.

In other words redo-ifchange, redo-icreate and redo-always links are redundant, everything may be done with redo itself.


### "Imaginary" target

One feature of this implementation is the possible use of nameless target

    redo ''

    redo ./

    redo some-dir/

Of course such targets can not exist, but they may have the corresponding script, named ".do". If You want to build "" target, then ".do" script will be looked for in the target directory and all upper dirs, without any "default" prefixes applicable. Such "imaginary" target may be an interesting replace for "all" target. The difference is in "default" rules, which are applicable for "all" target, but are ignored for "" target.


### Options available

-f

-x

-s list source files' full paths to stdout

-t list target files' full paths to stdout


### Loop dependencies

Are monitored unconditionally and issue error if found.


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

