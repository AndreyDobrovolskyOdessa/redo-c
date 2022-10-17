DEPS="t11 t22" . ../take.two

I=1

while [ $I -lt 70000 ]
do
	I=$(( $I + 1 ))
done

cat t11 t22 > $3

