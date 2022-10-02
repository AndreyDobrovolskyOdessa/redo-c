redo t12 t22

I=1

while [ $I -lt 60000 ]
do
	I=$(( $I + 1 ))
done

cat t12 t22 > $3

