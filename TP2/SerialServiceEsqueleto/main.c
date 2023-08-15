/**
* @file serial_service.c
* @Author Israel Pavelek
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "SerialManager.h"

/*Parametros del Socket*/

#define TCP_PORT        (10000)
#define IP_ADDRESS      "127.0.0.1"

/*Estructuras del socket*/

typedef enum{
   SOCKET_CLOSE=0,SOCKET_OPEN
}socketState_t;


typedef struct{
   char * ipAddress;
   int port;
   int listenFd;
   int connFd;
   socketState_t socketState;
}socket_t;

/*Parametros del Serial*/

#define SERIAL_PORT     1
#define SERIAL_BAUDRATE 115200
#define SERIAL_ERROR    0  
#define SERIAL_PERIOD   (100000)
#define open_port(x)    set_serial_estate(x,SERIAL_OPEN);
#define close_port(x)   set_serial_estate(x,SERIAL_CLOSE);

/*Estructuras del Serial*/
typedef enum{
   SERIAL_CLOSE=0,SERIAL_OPEN
}portState_t;

typedef struct{
   int port;
   int baudrate;
   portState_t portState;

}serialPort_t;

/*Estructura del bridge TCP-Serial <-> Serial-TCP*/

typedef struct{
   serialPort_t serial;
   socket_t socket_;
}bridge_t;


bool running = true;

/*pthread_t */

pthread_t serial_to_tcp_thread;
pthread_mutex_t mutex_serial = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tcp = PTHREAD_MUTEX_INITIALIZER;

/*Funciones del software*/

static void signals_set(void);
static void start_serial(serialPort_t *);
static void set_serial_estate(serialPort_t*,portState_t);
static portState_t get_serial_estate(serialPort_t*);
static void set_tcp_state(socket_t*,socketState_t);
static socketState_t get_tcp_state(socket_t);
static void signal_handler(int);
static void block_signals(bool);
static void start_tcp_server(socket_t*);
static void create_threads(bridge_t *);
static void tcp_to_serial(bridge_t *);
static void *serial_to_tcp(void *);

/*main funtion*/

int main(void) {
   bridge_t bridge ={{SERIAL_PORT,SERIAL_BAUDRATE,SERIAL_CLOSE},{IP_ADDRESS,TCP_PORT,0,0,0}};

   signals_set();                            
   printf("Comenzando el Serial Service\n\r");
   start_serial(&bridge.serial);             //apertura del puerto serie
   start_tcp_server(&bridge.socket_);        //apertura del socket TCP
   block_signals(true);                      //Bloqueo de señales para crear los threads
   create_threads(&bridge);                  //creacion del thread para recepcion serial 
   block_signals(false);
   tcp_to_serial(&bridge);                   
   if (0 !=pthread_cancel(serial_to_tcp_thread)) {
      printf("pthread_cancel\n\r");
      exit(EXIT_FAILURE);
   }
   if (0 != pthread_join(serial_to_tcp_thread, NULL)) {
      printf("pthread_join\n\r");
      exit(EXIT_FAILURE);
   }
   serial_close();
   printf("Threads terminados\n\r");
   return EXIT_SUCCESS;
}

/**
* @brief funtion to get TCP state
* @Author Israel Pavelek
* @param socket_t 
* @retval socketState_t
*/

socketState_t get_tcp_state(socket_t socket_) {
   pthread_mutex_lock(&mutex_tcp);                       //proteje con mutex
   socketState_t state=socket_.socketState;
   pthread_mutex_unlock(&mutex_tcp);
   return state;
}


/**
* @brief funtion to set TCP state
* @Author Israel Pavelek
* @param socket_t 
* @param socketState_t
* @retval none
*/
void set_tcp_state(socket_t* socket_,socketState_t state) {
   pthread_mutex_lock(&mutex_tcp);                      //proteje con mutex
   socket_->socketState=state;
   pthread_mutex_unlock(&mutex_tcp);
}

/**
* @brief funtion to get Serial state
* @Author Israel Pavelek
* @param serialPort_t 
* @retval serialPort_t
*/
portState_t get_serial_estate(serialPort_t* serial){
   pthread_mutex_lock(&mutex_serial);                    //proteje con mutex
   portState_t state=serial->portState;
   pthread_mutex_unlock(&mutex_serial);
   return state;
}


/**
* @brief funtion to set Serial state
* @Author Israel Pavelek
* @param serialPort_t 
* @param portState_t
* @retval none
*/

void set_serial_estate(serialPort_t* serial,portState_t state) {
   pthread_mutex_lock(&mutex_serial);
   serial->portState=state;
   pthread_mutex_unlock(&mutex_serial);
}

/**
* @brief signal_handler
* @Author Israel Pavelek
* @param int signal 
* @retval none
*/

void signal_handler(int sig) {
   
   if(sig == SIGINT){
      write(1, "Received SIGINT\n", strlen("Received SIGINT\n"));
   }else{
      write(1, "Received SIGTERM\n", strlen("Received SIGTERM\n"));
   }   
   write(1, "Cerrando el Serial Service...\n", strlen("Cerrando el Serial Service...\n"));
   running = false;
}

/**
* @brief signal_set
* @Author Israel Pavelek
* @param none
* @retval none
*/

void signals_set(void) {
   struct sigaction action;   
   action.sa_handler=signal_handler;
   sigemptyset(&action.sa_mask);   
   if (0> (sigaction(SIGINT, &action, NULL))) {
      perror("SIGINT sigaction");
      exit(EXIT_FAILURE);
   }
   if (0 > (sigaction(SIGTERM, &action, NULL))) {
      perror("SIGTERM sigaction");
      exit(EXIT_FAILURE);
   }
}

/**
* @brief block_signals
* @Author Israel Pavelek
* @param bool 
* @retval none
*/

void block_signals(bool block) {
   sigset_t set;
   int sigaction_code_error;
   sigemptyset(&set);
   sigaddset(&set, SIGTERM);
   sigaddset(&set, SIGINT);
   if(block){
      sigaction_code_error = pthread_sigmask(SIG_BLOCK, &set, NULL);
   }else{
      sigaction_code_error = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
   }
   if (0!=sigaction_code_error) {
      printf("pthread_sigmask: %s \n\r", strerror(sigaction_code_error));
      exit(EXIT_FAILURE);
   }
}

/**
* @brief start_serial funtion to start serial PORT
* @Author Israel Pavelek
* @param serialPort_t  *
* @retval none
*/

void start_serial(serialPort_t * serial) {
   printf("Abriendo el puerto serie\n\r");
   if(SERIAL_ERROR !=(serial_open(serial->port,serial->baudrate))) {
      printf("Error al abrir el puerto\n\r");
      exit(EXIT_FAILURE);
   }
   open_port(serial);
}

/**
* @brief start_tcp_server funtion to start TCP Server
* @Author Israel Pavelek
* @param socket_t *
* @retval none
*/

void start_tcp_server(socket_t* socket_) {
   
   struct sockaddr_in server = {
      .sin_family = AF_INET,
      .sin_port = htons(socket_->port)
   };

   printf("Inicializando el servidor TCP\n\r");
   socket_->listenFd = socket(AF_INET, SOCK_STREAM, 0);

   if (0>(socket_->listenFd)) {
      perror("socket");
      exit(EXIT_FAILURE);
   }
 
   // Crea una estructura de dirección de red
   if (0>(inet_pton(AF_INET, socket_->ipAddress, &server.sin_addr))) {
      perror("inet_pton");
      exit(EXIT_FAILURE);
   }

   if (0>(bind(socket_->listenFd, (struct sockaddr *) &server, sizeof(server)))) {
      perror("bind");
      exit(EXIT_FAILURE);
   }
	// Seteamos socket en modo Listening
   if (0>(listen(socket_->listenFd, 10))) {
      perror("inet_pton");
      exit(EXIT_FAILURE);
   }
}

/**
* @brief launch_threads 
* @Author Israel Pavelek
* @param bridge_t * 
* @retval none
*/

void create_threads(bridge_t * bridge1) {
   printf("Lanzando los threads\n\r");
   printf("%s\n\r",bridge1->socket_.ipAddress);

   if(0!=(pthread_create(&serial_to_tcp_thread, NULL, serial_to_tcp, (void*)bridge1))){
      printf("Error al crear el thread serial_to_tcp\n\r");
      exit(EXIT_FAILURE);
   }
} 

/**
* @brief tcp_to_serial funtion to send serial the data recieved from TCP 
* @Author Israel Pavelek
* @param bridge_t *
* @retval none
*/

void tcp_to_serial(bridge_t * bridge1) {
   char ipClient[24]={0};
   printf("Loop tcp_to_serial\n\r");

   while (running) {
      struct sockaddr_in client = {0};
      socklen_t client_len = sizeof(client);
      
      printf("Esperando conexión...\n\r");
      bridge1->socket_.connFd = accept(bridge1->socket_.listenFd, (struct sockaddr *) &client, &client_len);

      if (0>(bridge1->socket_.connFd)) {
         set_tcp_state(&bridge1->socket_,false);
         perror("Conexión abierta.\n\r");
      } else {
         set_tcp_state(&bridge1->socket_,true);
         inet_ntop(AF_INET, &client.sin_addr, ipClient, sizeof(ipClient));
         printf("Conexión desde: %s:%u \n\r", ipClient, ntohs(client.sin_port));
      }

      while (get_tcp_state(bridge1->socket_)) {
         char readBuffer[32];
         ssize_t readBytes = read(bridge1->socket_.connFd, readBuffer, sizeof(readBuffer));

         if (0>readBytes) {
            perror("tcp_to_serial: read");
            set_tcp_state(&bridge1->socket_,false);
         }else if (!readBytes) {
            printf("Conexión cerrada\n\r");
            set_tcp_state(&bridge1->socket_,false);
         }else{
            readBuffer[readBytes] = 0;
            printf("Recibí via TCP: %s \n\r", readBuffer);
            if (get_serial_estate(&bridge1->serial)) {
               serial_send(readBuffer, readBytes);
            } else {
               printf("El puerto no está abierto\n\r");
            }
         }
      }
      close(bridge1->socket_.connFd);
   }

   printf("cerrando \n\r");
}

/**
* @brief thread to to send TCP the data recieved from Serial
* @Author Israel Pavelek
* @param void *   (bridge_t *)
* @retval none
*/

void *serial_to_tcp(void *pv) {
   
   bridge_t * bridge1= (bridge_t*) pv;
   printf("Comenzando el thread Serial\n\r");
   printf("%s\n\r",bridge1->socket_.ipAddress);
   while (true) {
      char readBuffer[50];
      int readBytes = serial_receive(readBuffer, sizeof(readBuffer));
      if (readBytes == 0) {
         close_port(&bridge1->serial);
      }
      // Si tengo datos los muestro y envio via TCP
      if (readBytes > 0) {
         readBuffer[readBytes] = 0;
         printf("Recibí del puerto serie: %s \n\r", readBuffer);

         if (get_tcp_state(bridge1->socket_)) {
            write(bridge1->socket_.connFd, readBuffer, strlen(readBuffer));
         } else {
            printf("Conexion TCP no abierta\n\r");
         }
      }
      usleep(SERIAL_PERIOD);
   }

   return NULL;
}
