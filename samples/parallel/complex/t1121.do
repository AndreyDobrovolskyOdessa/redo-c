DEPS="t11 t21" . ../take.two

I=1

while [ $I -lt 40000 ]
do
	I=$(( $I + 1 ))
done

cat t11 t21 > $3

