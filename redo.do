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
#
#	redo ''

if test -n "$REDO_LEVEL"
then # driven by redo
  redo redo.c
  cc -o "$3" redo.c
else # bootstrapping
  cc -o redo redo.c &&
  {
    test -z "$1" &&
    { test "$(which redo)" = "$PWD"/redo || PATH=$PWD:$PATH; } ||
    { test -d "$1" && mv -i redo "$1"; } &&
    (
      cd "${1:-.}"
      ln -sf redo redo-ifchange &&
      ln -sf redo redo-ifcreate &&
      ln -sf redo redo-always
    ) &&
    echo Ok || echo Error;
  }
fi
