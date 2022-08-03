# for i in `seq 30 39`
# do
#     echo "output: $i"
#     echo $1 > /sys/devices/system/cpu/cpu$i/online
# done

# for i in `seq 10 19`
# do
#     echo "output: $i"
#     echo $1 > /sys/devices/system/cpu/cpu$i/online
# done

echo 0 > /sys/devices/system/cpu/cpu10/online