depends-on seed

I=1

while [ $I -lt 100000 ]
do
	I=$(( $I + 1 ))
done

cat seed > $3

