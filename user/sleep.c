
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    //printf("You entered: %d", argc);
    if(argc!=2){
        fprintf(2, "Usage: sleep <seconds>\n");
        exit(1);
    }
    //printf("You entered: %d", argc);
    int val=atoi(argv[1])*10;
    
    if(val<0){
        fprintf(2, "wrong number \n");
        exit(1);
    }
    //printf("start sleep\n");
    sleep(val);
    //printf("end sleep\n");
    exit(0);
}