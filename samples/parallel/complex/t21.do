redo t2

I=1

while [ $I -lt 20000 ]
do
	I=$(( $I + 1 ))
done

cat t2 > $3

