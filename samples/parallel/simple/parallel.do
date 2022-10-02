CMD="redo t1 t2 t3 t4"

(
  REDO_LOCK_FD=
  REDO_RETRIES=14
  $CMD &
  $CMD &
  wait
)

$CMD

