TNAME=${1##*/}
TDIR=${1%$TNAME}

depends-on ${TDIR}.do..${TNAME}

JOBS=${JOBS:-2}

LOGS=$(lua -e "for i = 1, $JOBS do print(('$1.%d.log'):format(i)) end")

export MAP_DIR=$(test -n "$TDIR" && cd $TDIR; pwd)

for LOG in $LOGS
do
	redo -l $LOG -m $1 $2 &
done

wait

{ echo 'return {'; cat $LOGS; echo '}'; } > $1.log

lua log2map.lua $1.log > $3

