ARG=${2##*/}
WDIR=${2%$ARG}

FIBONACCI=0

if test $ARG -eq 1
then
  FIBONACCI=1
fi

if test $ARG -gt 1
then
  DEP1=${WDIR}$(( $ARG - 2 )).fibonacci
  DEP2=${WDIR}$(( $ARG - 1 )).fibonacci
  depends-on $DEP1 $DEP2
  FIBONACCI=$(( $(cat $DEP1) + $(cat $DEP2) ))
fi

echo $FIBONACCI > $3
