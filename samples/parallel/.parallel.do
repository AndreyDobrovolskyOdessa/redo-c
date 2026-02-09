#	Controlled by JOBS and QUIET variables. Logs can be found
#	in <target>.parallel.<N>.log files. If QUIET is not defined
#	then log contains stderr of the recipes. In case QUIET is
#	defined (for example QUIET=y) then the log will contain both
#	stdout and stderr.

TNAME=${1##*/}
TDIR=${1%$TNAME}

depends-on ${TDIR}.do..${TNAME}

export MAP_DIR=$(test -n "$TDIR" && cd $TDIR; pwd)

REDO_LOG_FD=

if test -f "$1"
then
	JOBS=${JOBS:-2}
else
	JOBS=1
	export MAXJOBS=y
fi

LOGS=$(lua -e "for i = 1, $JOBS do print(('$1.%d.log'):format(i)) end")

for LOG in $LOGS
do
	if test -n "$QUIET"
	then
		redo -l 1 -m $1 $2 >$LOG 2>&1 &
	else 
		redo -l 2 -m $1 $2 2>$LOG &
	fi
done

wait

lua log2map.lua $LOGS > $3

