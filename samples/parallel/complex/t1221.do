redo t12 t21

I=1

while [ $I -lt 50000 ]
do
	I=$(( $I + 1 ))
done

cat t12 t21 > $3
