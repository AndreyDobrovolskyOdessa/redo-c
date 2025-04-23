## log2lock.lua utility

### Usage

    lua log2lock.lua [ logfile [ ... ] ]

### Output

Lock file names.

### Exit code

1 if locks were found, otherwise 0.

### Example

    REDO_RETRIES=1 redo -l target.log target
    echo $?
    75
    LOCK=$(lua log2lock.lua target.log) || fuser -s $LOCK || rm -f $LOCK



