#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

int main (int argc, char *argv[]) {
    FILE *fp;
    char cmd_mount[300] = "echo ";
    char cmd_renice[300] = "renice -n -20 ";
    char path[1035];
    fp = popen("pgrep -u Tingjia -f python3", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }
    printf("%s\n", cmd_renice);
    /* Read the output a line at a time - output it. */
    while (fgets(path, sizeof(path), fp) != NULL) {
        path[strlen(path) - 1] = '\0';
        char * path_copy = strdup(path);
        strcat(cmd_mount, path);
        strcat(cmd_mount, " > /sys/fs/cgroup/cpuset/test/tasks");
        // printf("%s\n", cmd_renice);
        strcat(cmd_renice, path_copy);
        printf("%s\n", cmd_renice);

        // system(cmd_mount);
        system(cmd_renice);
        cmd_mount[5] = '\0';
        cmd_renice[14] = '\0';
    }
    pclose(fp);
    struct timeval tval_before, tval_after, tval_result;
    // system("echo 10-19,30-39 > /sys/fs/cgroup/cpuset/test/cpuset.cpus");
    // sleep(1);

    // system("sudo insmod sched_profiler.ko");
    // sleep(1);
    // gettimeofday(&tval_before, NULL);
    // Need to change (when hyperthread change)
    // system("echo 0-9,20-29  > /sys/fs/cgroup/cpuset/test/cpuset.cpus");
    // gettimeofday(&tval_after, NULL);

    // sleep(1);
    // char cmd_output[] = "cat /proc/sched_profiler > ";
    // strcat(cmd_output, "output_");
    // strcat(cmd_output, argv[1]);
    // strcat(cmd_output, "_");
    // strcat(cmd_output, argv[2]);
    
    // system(cmd_output);
    // system("sudo rmmod sched_profiler.ko");

    // timersub(&tval_after, &tval_before, &tval_result);

    // printf("writing to cpuset.cpus file using : %ld.%06ld seconds\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
}