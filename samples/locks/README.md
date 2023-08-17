## log2lock.lua utility

### Usage

    lua log2lock.lua [ logfile [ ... ] ]

### Output

Lock file names.

### Exit code

1 if locks were found, otherwise 0.

### Example

    redo -l target.log target
    echo $?
    2
    LOCK=$(lua log2lock.lua target.log) || fuser -s $LOCK || rm -f $LOCK


