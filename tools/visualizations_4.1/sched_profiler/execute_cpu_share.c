#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

int main (int argc, char *argv[]) {

    FILE *fp;
    char cmd_mount[] = "echo ";
    char path[1035];
    fp = popen("pgrep -u Tingjia -f python3", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(path, sizeof(path), fp) != NULL) {
        path[strlen(path) - 1] = '\0';
        strcat(cmd_mount, path);
        strcat(cmd_mount, " > /sys/fs/cgroup/cpu/test/tasks");
        system(cmd_mount);
        cmd_mount[5] = '\0';
    }
    pclose(fp);
    struct timeval tval_before, tval_after, tval_result;
    system("echo 250000 > /sys/fs/cgroup/cpu/test/cpu.cfs_period_us");
    system("echo 250000 > /sys/fs/cgroup/cpu/test/cpu.cfs_quota_us");
    sleep(1);

    system("sudo insmod sched_profiler.ko");

    gettimeofday(&tval_before, NULL);
    system("echo -1 > /sys/fs/cgroup/cpu/test/cpu.cfs_quota_us");
    gettimeofday(&tval_after, NULL);

    sleep(1);
    char cmd_output[] = "cat /proc/sched_profiler > ";
    strcat(cmd_output, "output_");
    strcat(cmd_output, argv[1]);
    strcat(cmd_output, "_");
    strcat(cmd_output, argv[2]);
    
    system(cmd_output);
    system("sudo rmmod sched_profiler.ko");

    timersub(&tval_after, &tval_before, &tval_result);

    printf("writing to cpuset.cpus file using : %ld.%06ld seconds\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
}