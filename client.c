#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>

#include "network.h"
#include "client.h"

#define MAX_COMMAND_SIZE 106
#define DEBUG /*printf("%s %d\n", __FILE__, __LINE__)*/

char recvBuff[1024];
char sendBuff[1024];
int sockfd = 0;

int main(int argc, char** argv){
	
	if(argc!=2){
		printf("ERROR: Invalid number of arguments.\n");
		exit(1);
	}
	
	int n = 0;	
	struct sockaddr_in serv_addr;
	
	//Initialize network settings
	memset(recvBuff, '0', sizeof(recvBuff));
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("ERROR: Could not create socket.\n");
		return 1;
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	
	//This attempts a connection with the server and retries every 3 seconds if
	//one is not established
	while(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0){
		printf("Establishing connection with server...\n");	
		sleep(3);	
	}	
	
	//This is the first read to make sure we are connected.
	if((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0){		
		recvBuff[n] = 0;
		if(fputs(recvBuff, stdout) == EOF){
			printf("ERROR: Fputs error.\n");
		}		
	}
	if(n < 0){
		printf("ERROR: Read error.\n");
	}
	
	//These are the two threads that the client runs.
	//Response output waits for responses from the server and shows them to the user
	pthread_t response_output;
	//Command input waits for input from the user and sends it to the client
	pthread_t command_input;
	
	//Sets up a signal handler to finish account sessions on exit
	signal(SIGINT, sigint_handler);
	
	if(pthread_create(&command_input, 0, command_input_thread, &sockfd) != 0){
		printf("ERROR: Failure launching command input thread.\n");
		exit(1);
	}
	if(pthread_create(&response_output, 0, response_output_thread, &sockfd) != 0){
		printf("ERROR: Failure launching response output thread.\n");
		exit(1);
	}
	
	pthread_join(command_input, NULL);
	printf("Client end.\n");
	return 0;
		
}

//This signal handler will send a finish command when the client shuts down
//This ensures that no account session is left active if a client shuts down
void sigint_handler(int sig){
	memset(sendBuff, '\0', strlen(sendBuff));
	strcpy(sendBuff, "finish");
	if ((send(sockfd,sendBuff, strlen(sendBuff),0))== -1) {
		printf("ERROR: Could not close client account session on exit.\n");
		close(sockfd);
		exit(1);
    }
	exit(0);
}

//Helper function to check if a string is blank
int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

//This is the thread that waits for responses from the server
//If the response is "end" then the client will exit
void* response_output_thread(void* fd){
	int sockfd = *(int *)fd;
	while(1){
		memset(recvBuff, '\0', strlen(recvBuff));
		int num = recv(sockfd, recvBuff, sizeof(recvBuff), 0);
		if(num <= 0){
			printf("ERROR: Connection closed.\n");
			break;
		}
		
		if(strcmp(recvBuff, "end") == 0){
			printf("Client closing.\n");
			exit(0);
		}
		if(is_empty(recvBuff) == 0){
			printf("%s\n", recvBuff);
		}
	}
	return NULL;
}

//This thread takes input from the user and sends it to the server
void* command_input_thread(void* fd){
	int sockfd = *(int *)fd;
	while(1){
		DEBUG;
		printf("Input instruction:\n");
		
		char command[MAX_COMMAND_SIZE];
		memset(command, '\0', MAX_COMMAND_SIZE);
		
		memset(sendBuff, '\0', strlen(sendBuff));
		
		//These are used in extracting the operation and argument from the users input
		char op[100];
		char arg[100];
		char err[100];
		fgets(command, MAX_COMMAND_SIZE, stdin);
		
		if ((strlen(command)>0) && (command[strlen (command) - 1] == '\n')){
			command[strlen (command) - 1] = '\0';
		}
		
		int paramassigned = sscanf(command, "%s %s %s", op, arg, err);
		
		//If there are two arguments then they will be concatenated
		if(paramassigned == 2){
			strcat(op, " ");
			strcat(op, arg);
		//If there are more than two arguments then the command is invalid.			
		} else if(paramassigned > 2){
			printf("ERROR: Command not recognized.\n");
			continue;
		}
		DEBUG;
		DEBUG;
		//Write the user command to the buffer
		strcpy(sendBuff, op);
		DEBUG;
		
		//Send the buffer to the server
		if ((send(sockfd,sendBuff, strlen(sendBuff),0))== -1) {
                printf("ERROR: Could not send message.\n");
                close(sockfd);
                exit(1);
        }
        //Throttle the users command by waiting two seconds
        sleep(2);
	}
	return NULL;
}
