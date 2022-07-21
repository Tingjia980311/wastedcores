import sys
cpu_n = sys.argv[1]
file_id = sys.argv[2]

with open('output_' + str(cpu_n) + '_' + str(file_id), 'r') as f:
    lines = f.readlines()
num_cpu = 40
schedule_times = []
runqlens = {}
start_t = 0
LB_times_0 = []
LB_times_1 = []
LB_times_2 = []
LB_times_3 = []
LB_times_4 = []
LB_CPUS_0 = []
LB_CPUS_1 = []
LB_CPUS_2 = []
LB_CPUS_3 = []
LB_CPUS_4 = []
WF_times = []
for l in lines:
    datas = l.split(' ')
    while('' in datas):
        datas.remove('')
    while('\t' in datas):
        datas.remove('\t')
    if len(schedule_times) == 0:
        start_t = int(datas[num_cpu])
    # Decrease
    # for i in range(len(datas)): 
    #     if datas[i] == '-1': datas[i] = '1'
    # Increase cpuset
    # Need to change
    # print(datas)
    if len(datas) >= 40:
        if datas[0] == '-1': datas[0] = '40'
        if datas[42] == 'RQ':
            runqlens[int(datas[num_cpu]) - start_t] = [int(runqlen) for runqlen in datas[0:num_cpu]]
            schedule_times.append(int(datas[num_cpu]) - start_t)
    else:
        if datas[2] == 'WF\n':
            WF_times.append(int(datas[0]) - start_t)
        if datas[2] == 'RB':
            # if int(datas[num_cpu]) - start_t - LB_times[-1] >= 100000:
            # LB_times.append(int(int(datas[num_cpu]) - start_t)/1000)
            # print(datas[44])
            RB_CPU = int(datas[3])
            # Need to change (when hyperthread changes)
            if RB_CPU > 19: RB_CPU = RB_CPU - 10
            if (int(datas[4]) == 0):
                LB_times_0.append(int(int(datas[0]) - start_t)/1000)
                LB_CPUS_0.append(RB_CPU)
            if (int(datas[4]) == 1):
                LB_times_1.append(int(int(datas[0]) - start_t)/1000)
                LB_CPUS_1.append(RB_CPU)
            if (int(datas[4]) == 2):
                LB_times_2.append(int(int(datas[0]) - start_t)/1000)
                LB_CPUS_2.append(RB_CPU)
            if (int(datas[4]) == 3):
                LB_times_3.append(int(int(datas[0]) - start_t)/1000)
                LB_CPUS_3.append(RB_CPU)
            if (int(datas[4]) == 4):
                LB_times_4.append(int(int(datas[0]) - start_t)/1000)
                LB_CPUS_4.append(RB_CPU)

split_times = {}
split_no = 0
unit_times = []

for t in schedule_times:
    if int(t / 1000) != split_no:
        split_times[split_no] = unit_times
        split_no = int(t / 1000)
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
        if t < unit * 1000:
            split_times[unit][i] = unit * 1000
    split_times[unit].append(unit * 1000 + 1000)
    intval = []
    for i, t in enumerate(split_times[unit][0:-1]):
        intval.append(split_times[unit][i+1] - t)

    qlens_avg = []
    for i in range(len(qlens_unit[0])):
        qlen_avg = 0
        for j in range(len(intval)):
            qlen_avg += qlens_unit[j][i] * intval[j] / 1000
        if qlen_avg < 0: qlen_avg = 0
        # option: avoid very long runqueue overlooking the detail for lens changing in a low range in the heatmap
        if qlen_avg > 5: qlen_avg = 5
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
print(cpu_qlens_matrix.shape)
cpu_qlens_matrix = np.transpose(cpu_qlens_matrix)
# cpu_list = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39]
cpu_list = [0,1,2,3,4,5,6,7,8,9,20,21,22,23,24,25,26,27,28,29]

cpu_qlens_matrix = cpu_qlens_matrix[cpu_list,:]
print(cpu_qlens_matrix.shape)
fig = plt.figure(facecolor='#FFFFFF', figsize=(40,8))


sns.set(rc = {'figure.figsize':(40, 8)}, font_scale=1.8)
ax = sns.heatmap(cpu_qlens_matrix, cmap="Blues")
# print(len(LB_CPUS_0))
plt.scatter(LB_times_0, LB_CPUS_0, color = 'red')
# plt.scatter(LB_times_1, LB_CPUS_1, color = 'red')
for WF_time in WF_times:
    plt.axvline(WF_time/1000 , color='red', linewidth = 1)
# print(LB_CPUS_1)
# plt.scatter(LB_times_2, LB_CPUS_2)
# plt.scatter(LB_times_3, LB_CPUS_2, color = 'red')
# plt.scatter(LB_times_4, LB_CPUS_4, color = 'red')
plt.savefig("output_" + cpu_n + "_" + file_id + "_level0.png")


