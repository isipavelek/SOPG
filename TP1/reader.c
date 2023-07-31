/**
* @file reader.c
* @Author Israel Pavelek
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>


#define FIFO_NAME "fifo"
#define LOG_DATA_FILENAME "log.txt"
#define LOG_SIGNALS_FILENAME "sign.txt"
#define DATA_IDENTIFIER "DATA:"
#define SIGNAL_IDENTIFIER "SIGN:"
#define ENTRY_LOG_TXT "Entry LOG "
#define EOS_TXT "End of sesion\n"
#define SOS_TXT "Start of sesion\n"
#define HEADER_LENGTH (sizeof(DATA_IDENTIFIER)-1)
#define BUFFER_SIZE (300)
#define ERROR -1
#define FIRST_LETTER (inputBuffer[0])
#define FIRST_DATA_PTR (&inputBuffer[HEADER_LENGTH])


void write_file(FILE * fd,char * data_ini,int32_t entryLogNum);
void close_file(FILE* fd);
FILE * create_log_file(FILE *fd,const char * filename);
/*main funtion*/
int main(void) {

   int32_t fifoFd,entryDataLogNum=0,entrySignalLogNum=0;
   ssize_t bytesRead;
   FILE *logDataFd,*logSignalsFd;
   char inputBuffer[BUFFER_SIZE];
   
   printf("Starting reader...\n");

   if (mknod(FIFO_NAME, S_IFIFO | 0666, 0) < 0) {
      perror("Error creating the fifo");
      if (errno != EEXIST) {
         exit(EXIT_FAILURE);
      }else{
         perror("Fifo already exists");
      }
   }

	printf("waiting for writers...\n");
   /* Open named fifo. Blocks until other process opens it */

   if ((fifoFd = open(FIFO_NAME, O_RDONLY)) < 0) {
	   perror("Error opening the fifo");
      exit(EXIT_FAILURE);
   }

   /* open syscalls returned without error -> other process attached to named fifo */
	printf("got a writer\n");

   /* Creating log file. */
   logDataFd=create_log_file(logDataFd,LOG_DATA_FILENAME);

   /* Creating signals file. */
   logSignalsFd=create_log_file(logSignalsFd,LOG_SIGNALS_FILENAME);

   /* Loop while bytes to read */
   do{      
      /* read data into local buffer */
      if((bytesRead = read(fifoFd, inputBuffer, sizeof(inputBuffer)))==ERROR){
         perror("Error reading the fifo\n");
         exit(EXIT_FAILURE);
      }else if (bytesRead!=0){
         if(FIRST_LETTER=='D'){
            inputBuffer[bytesRead] = 0;
            write_file(logDataFd,FIRST_DATA_PTR,entryDataLogNum++);
         } else if (FIRST_LETTER=='S') {
            inputBuffer[bytesRead] = '\n';
            inputBuffer[bytesRead + 1] = 0;
            write_file(logSignalsFd,FIRST_DATA_PTR,entrySignalLogNum++);
         }
         
      }
   }while (bytesRead > 0);
   printf("cerrando\n");
   close_file(logSignalsFd);
   close_file(logDataFd);

   return EXIT_SUCCESS;
}
/**
* @brief Funtion to write file
* @Author Israel Pavelek
* @param FILE *
* @param char * data
* @param int32_t Log Entry number
* @retval none
*/
void write_file(FILE * fd,char * data_ini,int32_t entryLogNum){
   fprintf(fd, ENTRY_LOG_TXT);
   fprintf(fd, "%d: ", entryLogNum);
   fprintf(fd, "%s", data_ini);
   fflush(fd);
}

/**
* @brief Funtion to close file
* @Author Israel Pavelek
* @param FILE *
* @retval none
*/

void close_file(FILE* fd){

   fprintf(fd, EOS_TXT);
   fflush(fd);

   if (fclose(fd) != 0) {
      perror("Error closing log file...");
      exit(EXIT_FAILURE);
   }
}
/**
* @brief Funtion to create a log file
* @Author Israel Pavelek
* @param FILE *
* @param char * filename
* @retval FILE *
*/

FILE * create_log_file(FILE *fd,const char * filename){
   
   if((fd = fopen(filename, "a"))==NULL){
      perror("Error opening log data file\n");
      exit(EXIT_FAILURE);
   }
   fprintf(fd, SOS_TXT);
   fflush(fd);

   return fd;
}