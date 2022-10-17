# Triple-purpose script.
#
# Test-drive:
#
#	. ./redo.do
#
# Kick-start:
#
#	. ./redo.do $HOME/.local/bin
#
# Build redo with redo:
#
#	redo redo

if test -n "$REDO_TRACK"
then # driven by redo
  depends-on redo.c local.cflags
  test -f local.cflags && CFLAGS="$(cat local.cflags)" || CFLAGS=""
  cc $CFLAGS -o "$3" redo.c
else # bootstrapping
  cc -o redo redo.c &&
  {
    test -z "$1" &&
    { test "$(which redo)" = "$PWD"/redo || PATH=$PWD:$PATH; } ||
    { test -d "$1" && mv -i redo "$1"; } &&
    (
      cd "${1:-.}"
      ln -sf redo depends-on
    ) &&
    echo Ok || echo Error;
  }
fi

