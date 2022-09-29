
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    printf("I am parent with pid %d\n", getpid());
    sleep(15);
    pid_t pid = fork();
    if (pid == 0) { // the child process will execute this
        printf("I am child with pid %d. I got return value from fork: %d\n", getpid(), pid);
        exit(0); // we exit here so the child process will not keep running
    } else { // the parent process will execute this
        printf("I am parent with pid %d. I got return value from fork: %d\n", getpid(), pid);
    }
}