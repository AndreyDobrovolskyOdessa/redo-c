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

Improve performance avoiding unnecessary targets scripts' execution. As deep as possible dive inside the dependencies tree and attempt to execute scripts placed deeper prior to those placed closer to the build root. Let's imagine, that A depends on B, and B depends on C (some source). Then redo_target() recurses down to C, checks its hash, and in case it was changed, runs B.do. Then B hash is checked and A.do is run then and only if B's hash differs from its previous value, stored in the .dep file.


### redo-ifchange

Meaning: redo-this-script-if-change-after-updating dependency1 ...

Undoubtedly is the heart of the "redo" build system. Its main property is the possibility to communicate with the parent, supplying it with the information (targets names, dates and hashes), sufficient for making decision about the parent's next possible invocation conditions.


### redo

Meaning: redo-this-script-even-if-not-change-after-updating dependency1 ...

In the current version "redo" act following the same pattern - build targets if they are outdated, supplies parent process with the information about their hashes, but tells the parent, that next time it (parent) is to be run unconditionally (not the targets scripts).

Here lies the incompatibility with many known "redo" implementations, because they treat "redo" command, like it must unconditionally force targets' scripts to be run. I guess "redo" command in these implementations can not be used in the .do scripts, because it don't send hashes to the parent and called from inside the .do script will brake the dependencies tree consistency. For this purpose traditionally (starting probably from A.Pennarun's implementation) "redo-always" is used.

So in this implementation "redo" can be used in place of "redo-always", meaning that the script, from which it was run, next time will be run unconditionally, but the targets to be built may be specified and their hashes will be stored in the corresponding .dep file. Instead of

    redo-ifchange a b c
    redo-always

You use

    redo a b c

If You want some .do script to be executed unconditionally, place in it

    redo

If You want the targets to be built unconditionally, use -f option

    redo-ifchange -f t1 t2 t3

If You want script to be run unconditionally and targets built unconditionally, use

    redo -f t1 t2 t3

D.J.Bernstein in his papers was talking mostly about "redo-ifchange", and didn't mentioned "redo-always". In my opinion "redo" command is to follow the "redo-ifchange" mode of operation - being used in the .do script, tie together the nodes (targets) into the consistent dependency tree. 


### redo-always

Meaning: redo-this-script-always-after-updating dependency1 ...

In current implementation is alias of "redo" command. Thanks for idea to Sergey Matveev, author of [goredo](http://www.goredo.cypherpunks.ru/), Go implementation of "redo" build system.


### redo-ifcreate

D.J.Bernstein proposed to use "redo-ifchange" for detecting of the target's change and removal, while "redo-ifcreate" for detecting the target's creating. The current imlementation of "redo-ifchange" provides all three tasks, so "redo-ifcreate" and "redo-ifchange" are fully interchangeable.


### Default "all" target

Not implemented, because it will break the use of "redo" without targets specified in the .do scripts.


### "Imaginary" target

One feature of this implementation is the possible use of nameless target

    redo ''

or

    redo ./

Of course such targets can not exist, but thay may have the corresponding script, named ".do". If You want to build '' target, then ".do" script will be looked for in the current directory and all upper dirs, without any "default" prefixes applied. Such non-existing targets may be interesting replace for "all" target. The difference is in "default" rules, which are applicable for "all" target, but are ignored for "" target.


### Notes on stdout and $3

Sorry, stdout is not captured. $3 is the only possible option. Someone treats this as the total fail. My bad, I am afraid that mixing stdout and $3 is mess. If You need stdout to be captured, You can easily do it for every script individually with the help of

    exec >$3

at the .do file top. This will allow flawlessly use in one build both approaches.


### Empty and missing targets

Are matching, because hashes of empty file and missing one are equal. If script don't write anything into $3, empty new target is not created.


### Current version usage

Only -f and -x options available.


### Loop dependencies

Are monitored unconditionally and issue error if found.


Andrey Dobrovolsky

