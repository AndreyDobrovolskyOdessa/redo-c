# Basic usage examples

## factorial

Factorial is traditional recursion test task.

    . ./redo.do
    cd samples/basic/factorial
    redo 10.factorial
    redo 20.factorial
    rm 7.factorial
    redo 20.factorial

## fibonacci

Fibonacci numbers grow slower than factorial.

    . ./redo.do
    cd samples/basic/fibonacci
    redo 20.fibonacci
    redo 40.fibonacci
    rm 17.fibonacci
    redo 35.fibonacci

