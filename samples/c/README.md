# C-oriented .do set

Allow to build projects with correctly written headers.

## Examples

### 1.

int bar(int) squares the integer\
int foo(int, int) retrns the sum of arguments' squares - call bar()\
int main() returns 1's and 2's squares sum - call foo()

In order to build the program "prog" You need to enter 1/prog and

    redo ''

Compiler gcc (see c/default.require.do)\
Linker gcc (see c/default.bin.do)\
CFLAGS="-Wall -Wextra" (see c/default.cflags.do)

for all sources

### 2.

The same task, but for all sources CFLAGS="-Os -Wall -Wextra" (see c/2/default.cflags.do), foo.c is compiled with tcc (see c/2/foo/default.require.do) and the resulting binary c/2/prog/prog is linked wit tcc (see c/2/prog/default.bin.do)

### 3.

main() calls baz() in order to print the sum of the screen width's and height's squares. baz() needs ncurses (see c/3/baz/default.cflags.do).


