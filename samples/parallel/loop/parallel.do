CMD="redo l1 l2"

(
  REDO_LOCK_FD=
  REDO_RETRIES=4
  $CMD &
  $CMD &
  wait
)

$CMD

