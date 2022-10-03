# Parallel builds examples

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

## Loop

This example demonstrates how lock-free approach allows to locate and avoid loop dependencies even in parallel builds.

Starting in redo-c git directory:

    . ./redo.do
    cd samples/parallel/loop
    redo parallel

