# C-oriented .do set

Allow to build projects with correctly written headers.

## Examples

### 1.

int bar(int) squares the integer\
int foo(int, int) returns the sum of arguments' squares - calling bar()\
int main() returns 1's and 2's squares sum - calling foo()

You can build the program "prog" with

    redo samples/c/1/prog/

or entering samples/c/1/prog and running

    redo ''

Compiler gcc (see samples/c/default.require.do)\
Linker gcc (see samples/c/default.bin.do)\
CFLAGS="-Wall -Wextra" (see samples/c/default.cflags.do)

for all sources

### 2.

The same task, but foo.c and main.c sources are to be compiled using CFLAGS="-Os -Wall -Wextra" (see samples/c/2/default.cflags.do), foo.c is compiled with tcc (see samples/c/2/foo/default.require.do) and the resulting binary samples/c/2/prog/prog is linked wit tcc (see samples/c/2/prog/default.bin.do). foo.c uses bar.c of Example 1 (see samples/c/2/foo/foo.c).

### 3.

main() calls baz() in order to print the sum of the screen width's and height's squares. baz() needs ncurses (see samples/c/3/baz/default.cflags.do). baz.c uses foo.c of Example 1 (see samples/c/3/baz/baz.c).


