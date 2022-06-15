for i in `seq 5 29`
do
    echo "output: $i"
    echo $1 > /sys/devices/system/cpu/cpu$i/online
done