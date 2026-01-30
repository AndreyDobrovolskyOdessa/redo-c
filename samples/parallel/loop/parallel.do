DEPS="l1 l2"

(
  redo $DEPS &
  redo $DEPS &
  wait
)

depends-on $DEPS

