# Triple-purpose script.
#
# Test-drive:
#
#	DESTDIR= ; . ./redo.do
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

if $(which redo >/dev/null)
then
  redo redo.c
  OFILE=${3:-redo}
fi

cc -O2 -Wall -Wextra -Wwrite-strings -o $OFILE redo.c &&

if [ x${OFILE} = xredo ]
then
  test -z "$DESTDIR" &&
  { DESTDIR=$PWD; PATH=$DESTDIR:$PATH; } || 
  cp -nT redo ${DESTDIR}/redo &&
  {
    ln -sf redo ${DESTDIR}/redo-ifchange
    ln -sf redo ${DESTDIR}/redo-ifcreate
    ln -sf redo ${DESTDIR}/redo-always
  }
fi

