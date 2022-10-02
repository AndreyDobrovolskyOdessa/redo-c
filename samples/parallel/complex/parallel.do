CMD="redo t1121 t1122 t1221 t1222"

(
  REDO_LOCK_FD=
  REDO_RETRIES=14
  $CMD &
  $CMD &
  wait
)

$CMD

