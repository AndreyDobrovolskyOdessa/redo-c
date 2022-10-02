redo seed

I=1

while [ $I -lt 10000 ]
do
	I=$(( $I + 1 ))
done

cat seed > $3

