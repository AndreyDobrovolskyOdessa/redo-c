ARG=${2##*/}
WDIR=${2%$ARG}

FACTORIAL=1

if test $ARG -gt 1
then
  DEP=${WDIR}$(( $ARG - 1 )).factorial
  depends-on $DEP
  FACTORIAL=$(( $(cat $DEP) * $ARG ))
fi

echo $FACTORIAL > $3
