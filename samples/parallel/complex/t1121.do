redo t11 t21

I=1

while [ $I -lt 40000 ]
do
	I=$(( $I + 1 ))
done

cat t11 t21 > $3

