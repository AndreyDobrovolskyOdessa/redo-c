(
  REDO_LOCK_FD=
  REDO_RETRIES=14
  redo t1 t2 t3 t4 &
  redo t1 t2 t3 t4 &
  wait
)

redo t1 t2 t3 t4

