redo t1

I=1

while [ $I -lt 90000 ]
do
	I=$(( $I + 1 ))
done

cat t1 > $3

