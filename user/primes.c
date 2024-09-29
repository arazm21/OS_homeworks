
#include "kernel/types.h"
#include "user/user.h"

void primes(int left_pipe[2]) __attribute__((noreturn));
void primes(int left_pipe[2]) {
    int prime;
    int currentNumber;
    if (read(left_pipe[0], &prime, sizeof(prime)) == 0) {
        close(left_pipe[0]);
        exit(0);
    }
    printf("prime %d\n", prime);
    int right_pipe[2];
    pipe(right_pipe);
    if (fork() == 0) {
        close(left_pipe[0]);
        close(right_pipe[1]);
        primes(right_pipe);
    } else {
        close(right_pipe[0]); 

        while (read(left_pipe[0], &currentNumber, sizeof(currentNumber)) != 0) {
            if (currentNumber % prime != 0) {
                write(right_pipe[1], &currentNumber, sizeof(currentNumber));
            }
        }
        close(left_pipe[0]);
        close(right_pipe[1]);
        wait(0);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    int pipefd[2];

    pipe(pipefd);

    int forkRes = fork();
    if(forkRes==-1){
        return -1;
    }  else if (forkRes == 0) {
        close(pipefd[1]);
        primes(pipefd);
    } else {
        close(pipefd[0]);

        for (int i = 2; i <= 280; i++) {
            write(pipefd[1], &i, sizeof(i));
        }
        close(pipefd[1]);
        wait(0);
        exit(0);
    }
}
