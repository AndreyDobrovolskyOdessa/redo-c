# Building targets in parallel examples

## Simple

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/simple
    rm .redo.*
    echo 1 > seed
    time redo sequential
    echo 2 > seed
    time redo sequential
    rm .redo.*
    echo 1 > seed
    time redo parallel
    echo 2 > seed
    time redo parallel


## Complex

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/complex
    rm .redo.*
    echo 1 > seed
    time redo sequential
    echo 2 > seed
    time redo sequential
    rm .redo.*
    echo 1 > seed
    time redo parallel
    echo 2 > seed
    time redo parallel


