(
  REDO_LOCK_FD=
  REDO_RETRIES=14
  redo t1121 t1122 t1221 t1222 &
  redo t1121 t1122 t1221 t1222 &
  wait
)

redo t1121 t1122 t1221 t1222

