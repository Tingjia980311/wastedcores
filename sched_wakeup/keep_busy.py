import time
import multiprocessing

n_cpu = 1

row_num = 3200
col_num = 320

a = []
for i in range(row_num):
    a.append(range(col_num))
b = range(col_num)

def calculate_single_element(min_i, max_i):
    start_t = time.time()
    c = []
    for m in range(10000):
        for j in range(min_i, max_i):
            c.append(0)
            for i in range(col_num):
                c[j - min_i] += a[j][i]*b[i]
    end_t = time.time()
    return c[100]

calculate_single_element(0,3200)