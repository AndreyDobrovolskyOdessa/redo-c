TNAME=${1##*/}
TDIR=${1%$TNAME}

depends-on ${TDIR}.do..${TNAME}

export MAP_DIR=$(test -n "$TDIR" && cd $TDIR; pwd)

if test -f "$1"
then
	JOBS=${JOBS:-2}

	LOGS=$(lua -e "for i = 1, $JOBS do print(('$1.%d.log'):format(i)) end")

	for LOG in $LOGS
	do
		redo -l $LOG -m $1 &
	done

	wait

else
	LOGS=$1.log

	JOBS="" redo -l $LOGS $2
fi

lua log2map.lua $LOGS > $3

