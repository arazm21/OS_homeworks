
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {



    
    if (argc < 2) {
        fprintf(2, "wrong\n");
        exit(1);
    }

    char buf[512];
    char *newArgv[64];
    int n = 0;
    for (int i = 1; i < argc; i++) {
        
        newArgv[n] = argv[i];
        //printf("%s\n",argv[i]);
        n++;
    }
    
    while (1) {
        int stringSize = 0;
        char currChar;
        while (1) {        
            int readInput =read(0, &currChar, sizeof(char));
            if(readInput <= 0 || currChar =='\n') break; 
            
            buf[stringSize] = currChar;
            stringSize++;
        }
        if (stringSize == 0) break;

        buf[stringSize] = '\0'; 
        newArgv[n] = buf; 
        newArgv[n + 1] = 0; 
        // for(int i = 0 ; i < n+1;i++){
        //     printf("%s\n",newArgv[i]);
        // }
        if (fork() == 0) {
            exec(newArgv[0], newArgv);
            fprintf(2, "exec failed\n");
            exit(1);
        } else {
            wait(0);
        }
    }

    exit(0);
}
