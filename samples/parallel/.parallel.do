TNAME=${1##*/}
BNAME=${2##*/}
TDIR=${1%$TNAME}

depends-on ${TDIR}.do..${TNAME}

JOBS=${JOBS:-2}

LOGS=$(lua -e "for i = 1, $JOBS do print(('$TNAME.%d.log'):format(i)) end")

RDIR=$(pwd)

test -n "$TDIR" && cd $TDIR

export MAP_DIR=$(pwd)

REDO_DIRPREFIX=

for LOG in $LOGS
do
	redo -l $LOG -m $TNAME $BNAME &
done

wait

{ echo 'return {'; cat $LOGS; echo '}'; } > $TNAME.log

cd $RDIR

lua log2map.lua $1.log > $3

