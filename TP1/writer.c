/**
* @file writer.c
* @Author Israel Pavelek
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#define FIFO_NAME "fifo"
#define DATA_IDENTIFIER   "DATA:"
#define HEADER_LENGTH (sizeof(DATA_IDENTIFIER)-1)
#define BUFFER_SIZE (300)
#define MSG_LENGHT (sizeof(inputBuffer) - HEADER_LENGTH)
#define EXIT_WORD "exit"
#define SIGNUSR1_MSG    "SIGN:1"
#define SIGNUSR2_MSG    "SIGN:2"
#define SIGNUSR1_LENGTH (sizeof(SIGNUSR1_MSG))
#define SIGNUSR2_LENGTH (sizeof(SIGNUSR2_MSG))
#define ERROR -1
#define MATCH 0


static int32_t fifoFd;

/**
* @brief signal hadler for SIGUSR1 y SIGUSR2
* @Author Israel Pavelek
* @param int signal
* @retval none
*/
void sigusr_handler(int sig) {
   if(sig==SIGUSR1){
      if(write(fifoFd, SIGNUSR1_MSG, SIGNUSR1_LENGTH)==ERROR){
         perror("SIGUSR1 error");
      }  
   }else{
      if(write(fifoFd, SIGNUSR2_MSG, SIGNUSR2_LENGTH)==ERROR){
         perror("SIGUSR2 error");
      }
   }
}

/*main funtion*/

int main(void) {
   bool exitSoft=false;
   struct sigaction sa_sigusr;
   char inputBuffer[BUFFER_SIZE];

   printf("Starting writer\n");

   /* Creating and opening a named FIFO. */

   if (mknod(FIFO_NAME, S_IFIFO | 0666, 0) < 0) {
      if (errno != EEXIST) {
         exit(EXIT_FAILURE);
      }else{
         perror("Fifo already exists");
      }
   }

   printf("Waiting for readers...\n");
   
   if((fifoFd = open(FIFO_NAME, O_WRONLY))<0){
      perror("open");
      exit(EXIT_FAILURE);
   }

   printf("Got a reader\n");

   /* Setting up singnal handlers. */

   sa_sigusr.sa_handler = sigusr_handler;
   sa_sigusr.sa_flags = SA_RESTART;
   sigemptyset(&sa_sigusr.sa_mask);
   
   if (sigaction(SIGUSR1, &sa_sigusr, NULL) < 0) {
      perror("SIGUSR1 sigaction");
      exit(EXIT_FAILURE);
   }

   if (sigaction(SIGUSR2, &sa_sigusr, NULL) < 0) {
      perror("SIGUSR2 sigaction");
      exit(EXIT_FAILURE);
   }

   /*Copy de DATA identifier into the string*/

   strcpy(inputBuffer, DATA_IDENTIFIER);
   
   /*repeat until type exit*/
   do {
      printf("Message to send (type \"exit\" to finish):");

      if (fgets(&inputBuffer[HEADER_LENGTH], MSG_LENGHT, stdin) == NULL) {
         perror("fgets");
         exit(EXIT_FAILURE);
      }

      if(strncmp(&inputBuffer[HEADER_LENGTH],EXIT_WORD,sizeof(EXIT_WORD)-1)==MATCH){
         exitSoft=true;
      }else{
         if (write(fifoFd, inputBuffer, strlen(inputBuffer)) < 0) {
            perror("write");
            exit(EXIT_FAILURE);
         }
      }
     
   }while(!exitSoft);

   return EXIT_SUCCESS;
}
