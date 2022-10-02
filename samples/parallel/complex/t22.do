redo t2

I=1

while [ $I -lt 30000 ]
do
	I=$(( $I + 1 ))
done

cat t2 > $3

