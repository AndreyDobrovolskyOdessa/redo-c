# Dual-purpose script.
#
# Kick-start:
#
#	(DESTDIR=$HOME/.local/bin ; . ./redo.do)
#
# Build redo with redo:
#
#	redo redo
#
#	redo ''

OFILE=redo

if $(which redo-ifchange >/dev/null)
then
  redo-ifchange redo.c
  OFILE=${3:-redo}
fi

cc -g -Os -Wall -Wextra -Wwrite-strings -o $OFILE redo.c

if [ x${OFILE} = xredo ]
then
  test -n "$DESTDIR" || DESTDIR=${HOME}/.local/bin
  cp -nT redo ${DESTDIR}/redo && {
    ln -sf redo ${DESTDIR}/redo-ifchange
    ln -sf redo ${DESTDIR}/redo-ifcreate
    ln -sf redo ${DESTDIR}/redo-always
  }
fi

