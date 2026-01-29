TNAME=${1##*/}
TDIR=${1%$TNAME}

depends-on ${TDIR}.do..${TNAME}

export MAP_DIR=$(test -n "$TDIR" && cd $TDIR; pwd)

REDO_LOG_FD=

if test -f "$1"
then
	JOBS=${JOBS:-2}

	LOGS=$(lua -e "for i = 1, $JOBS do print(('$1.%d.log'):format(i)) end")

	for LOG in $LOGS
	do
		if test -n "$QUIET"
		then
			redo -l 1 -m $1 >$LOG 2>&1 &
		else 
			redo -l 2 -m $1 2>$LOG &
		fi
	done

	wait

else
	LOGS=$1.log

	if test -n "$QUIET"
	then
		JOBS= redo -l 1 $2 >$LOGS 2>&1
	else 
		JOBS= redo -l 2 $2 2>$LOGS
	fi
fi

lua log2map.lua $LOGS > $3

