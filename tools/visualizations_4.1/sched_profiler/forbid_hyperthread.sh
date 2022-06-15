for i in `seq 4 5`
do
    echo "output: $i"
    echo $1 > /sys/devices/system/cpu/cpu$i/online
done