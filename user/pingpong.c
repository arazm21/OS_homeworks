// user/pingpong.c
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int p1[2]; 
    int p2[2]; 
    char buffer[1]; 
    if(pipe(p1)==-1){
        printf("error1\n");
        return 1;
    }
    if(pipe(p2)==-1){
        printf("error2\n");
    }

    int pid = fork();

    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    } else if (pid == 0) {

        close(p1[1]);
        close(p2[0]);


        read(p1[0], buffer, 1);
        printf("%d: received ping\n", getpid());

        write(p2[1], buffer, 1);

        close(p1[0]);
        close(p2[1]);
        exit(0);
    } else {
        close(p1[0]);
        close(p2[1]); 

        buffer[0] = 'x';
        write(p1[1], buffer, 1);

        read(p2[0], buffer, 1);
        printf("%d: received pong\n", getpid());

        close(p1[1]);
        close(p2[0]);

        wait(0);

        exit(0);
    }
}
