source venv/bin/activate
for i in `seq 1 12`
do
    sudo ./a.out 4 $i
    python3 plot.py 4 $i
    echo $i
done