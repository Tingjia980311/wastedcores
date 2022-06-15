import os
import time

pids_pipe = os.popen("pgrep -u Tingjia -f python3")
pids = pids_pipe.read()
pids_pipe.close()

pids = pids.split('\n')
cur_pid = os.getpid()

if str(cur_pid) in pids:
    pids.remove(str(cur_pid))
if str(int(cur_pid) + 1) in pids:
    pids.remove(str(int(cur_pid) + 1))
pids.remove("")
print(sorted(pids))
# subprocess.call(["mkdir", "/sys/fs/cgroup/cpuset/test"])
for pid in pids: 
    os.system("echo " + str(pid) + " > /sys/fs/cgroup/cpuset/test/tasks")
os.system('echo 0 > /sys/fs/cgroup/cpuset/test/cpuset.cpus')
time.sleep(1)

os.system('sudo insmod sched_profiler.ko')
# os.system('echo 0-39  > /sys/fs/cgroup/cpuset/test/cpuset.cpus')
time.sleep(1)
os.system('cat /proc/sched_profiler > output')
os.system('sudo rmmod sched_profiler.ko')
