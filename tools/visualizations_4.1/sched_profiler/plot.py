import sys
cpu_n = sys.argv[1]
file_id = sys.argv[2]

with open('output_' + str(cpu_n) + '_' + str(file_id), 'r') as f:
    lines = f.readlines()
num_cpu = 64
schedule_times = []
runqlens = {}
start_t = 0
for l in lines:
    datas = l.split(' ')
    while('' in datas):
        datas.remove('')
    if len(schedule_times) == 0:
        start_t = int(datas[num_cpu])
    if datas[0] == '-1': datas[0] = 0
    runqlens[int(datas[num_cpu]) - start_t] = [int(runqlen) for runqlen in datas[0:num_cpu]]
    schedule_times.append(int(datas[num_cpu]) - start_t)


split_times = {}
split_no = 0
unit_times = []

for t in schedule_times:
    if int(t / 100000) != split_no:
        split_times[split_no] = unit_times
        split_no = int(t / 100000)
        if t == 0:
            unit_times = [t]
        else:
            unit_times = [unit_times[-1], t]
    else:
        unit_times.append(t)

j = 0
cpu_qlens_over_time = {}
for unit in split_times.keys():
    qlens_unit = []
    for i, t in enumerate(split_times[unit]):
        qlens_unit.append(runqlens[t])
        if t < unit * 100000:
            split_times[unit][i] = unit * 100000
    split_times[unit].append(unit * 100000 + 100000)
    intval = []
    for i, t in enumerate(split_times[unit][0:-1]):
        intval.append(split_times[unit][i+1] - t)

    qlens_avg = []
    for i in range(len(qlens_unit[0])):
        qlen_avg = 0
        for j in range(len(intval)):
            qlen_avg += qlens_unit[j][i] * intval[j] / 100000
        if qlen_avg < 0: qlen_avg = 0
        # option: avoid very long runqueue overlooking the detail for lens changing in a low range in the heatmap
        if qlen_avg > 3: qlen_avg = 3
        qlens_avg.append(qlen_avg)
    cpu_qlens_over_time[unit] = qlens_avg

cpu_qlens_matrix = []
for i in range(max(cpu_qlens_over_time.keys())):
    if i not in cpu_qlens_over_time.keys():
        cpu_qlens_over_time[i] = cpu_qlens_over_time[i-1]
    cpu_qlens_matrix.append(cpu_qlens_over_time[i])

import re
import matplotlib.pyplot as plt
import numpy as np
# import heatmap
import seaborn as sns;


cpu_qlens_matrix = np.array(cpu_qlens_matrix)
cpu_qlens_matrix = np.transpose(cpu_qlens_matrix)
cpu_list = [0,1,2,3]
cpu_qlens_matrix = cpu_qlens_matrix[cpu_list,:300]
print(cpu_qlens_matrix.shape)
sns.set(rc = {'figure.figsize':(40, 8)}, font_scale=1.8)
ax = sns.heatmap(cpu_qlens_matrix, cmap="Blues")
plt.savefig("output_" + cpu_n + "_" + file_id + ".png")


