#	Controlled by JOBS and QUIET variables. Logs can be found
#	in <target>.parallel.<N>.log files. If QUIET is not defined
#	then log contains stderr of the recipes. In case QUIET is
#	defined (for example QUIET=y) then the log will contain both
#	stdout and stderr.

depends-on .do..$1

REDO_LOG_FD=

if test -f "$1"
then
	JOBS=$(lua -e "print(math.floor(tonumber('$JOBS') or 2))")

	if test "$JOBS" -lt 1
	then
		JOBS=1
	fi

	if test "$JOBS" -ge 2
	then
		MAXJOBS=
	fi
else
	JOBS=1
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

MAP_DIR=$(pwd) lua ${4}log2map.lua $LOGS > $3

