#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{   
    // char str[2];
    // str[1] = '\0';
    int bufferLength = 2048*8*16;
    char* buffer = sbrk(bufferLength); 
    //int found = 0;
    //int startWriting = 0;
    //char secret[100];
    char startOfString[]="very very";

     
        // memmove(secret,&buffer[32],8);
        // // secret[8] = '\0';

        // write(2, secret, 9);
        // exit(0);
      

    for(int i = 0; i < bufferLength; i++){
       //str[0]=buffer[i];
       //printf("%s",str);
      // if(found==1 && buffer[i]!=' ')startWriting=1;
      // if(buffer[i] == ':')found = 1;
      // if(startWriting == 1){
      //   memcpy(secret,&buffer[i],8);
      //   secret[8] = '\0';

      //   write(2, secret, 8);
      //   break;
      // }
      int res=memcmp(startOfString,&buffer[i],8);
      if(res==0){
        //printf("fffffffffffff");
        char secret[8] ;
        memmove(secret,&buffer[i+32-8],8);
        secret[7]='\0';
        write(2, secret, 8);
        //printf("%s",secret);
      }
        
    }
    
    exit(0);
}
