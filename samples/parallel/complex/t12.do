redo t1

I=1

while [ $I -lt 110000 ]
do
	I=$(( $I + 1 ))
done

cat t1 > $3

