#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <pthread.h>
#include <semaphore.h>

#include <time.h>

#include <signal.h>

#include "server.h"
#include "account.h"
#include "network.h"

#define DEBUG /*printf("%s %d\n", __FILE__, __LINE__)*/

account** bank;
int numAccounts;

pthread_mutex_t account_mutexes[20];
pthread_mutex_t open_account_mutex;

fdnode* head = NULL;

int main(int argc, char **argv){	
	//This initializes the bank
	bank = newBank();
	numAccounts = 0;
	//The client acceptor thread listens for clients
	pthread_t client_acceptor;
	//The print thread prints the balances every 20 seconds
	pthread_t print;
	//This signal handler will wait for the server to receive SIGINT so it can shut the clients down
	signal(SIGINT, sigint_handler);
	//Start the client acceptor thread
	if(pthread_create(&client_acceptor, 0, client_acceptor_thread, 0) != 0){
		printf("ERROR: Could not start server.\n");
		exit(EXIT_FAILURE);
	}
	//Start the printing thread
	pthread_create(&print, 0, print_thread, 0);
	pthread_join(client_acceptor, NULL);
	
	printf("SERVER: Server end.\n");
	return 0;
}

//This thread will listen for clients over the designated port (defined in network.h)
void* client_acceptor_thread(void* arg){

	int connfd = 0;
	int listenfd = 0;
	char sendBuff[1025];
	
	struct sockaddr_in serv_addr;
	
	//Initialize network listeners
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, '0', sizeof(serv_addr));
	memset(sendBuff, '\0', sizeof(sendBuff));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

	bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	if(listen(listenfd, 10) ==-1){
		printf("ERROR: Listen error.\n");
		return NULL;
	}
	
	//This loop is what actually waits for the clients to connect
	while(1){
		
		if(listen(listenfd, 10) ==-1){
			printf("ERROR: Listen error.\n");
		}
		
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
		strcpy(sendBuff, "SERVER: Connection to server successful.\n");
		printf("SERVER: Connection to client successful.\n");
		write(connfd, sendBuff, strlen(sendBuff));
		
		//On connection, open new thread to speak with this client
		pthread_t client_service;
		
		if(pthread_create(&client_service, 0, client_service_thread, &connfd) != 0){
			printf("ERROR: Failure launching client-service thread.\n");
			sleep(1);
		}		
	}
	return NULL;
}

//This thread is what maintains communication with the client
void* client_service_thread(void* arg){
	int fd = *(int *)arg;	
	//Add this file descriptor to our list of current clients
	add(fd);
	
	char accountname[1024] = {'\0'};
	
	//This loop is what actually speaks with the client back and forth
	while(1){
		int num;
		//int size = sizeof(struct sockaddr_in);
		
		char buffer[1024] = {'\0'};
		
		//Wait for a message in the buffer
		if ((num = recv(fd, buffer, 1024,0)) == -1) {
			printf("ERROR: Could not receive data from client.\n");
			break;
		} else if (num == 0) {
			printf("SERVER: Connection closed with client.\n");
			break;
		}
		
		buffer[num] = '\0';
		printf("SERVER: Received from client: %s.\n", buffer);
		
		//Extract arguments from the buffer
		char op[100];
		char sarg[100] = {'\0'};
		sscanf(buffer, "%s %s", op, sarg);		
		
		//Send the args to the interact function which actually processes the command
		//This ensures encapsulation. All this thread does is take args from the client
		//and send it to interact
		memset(buffer, '\0', 1024);
		interact(op, sarg, accountname, buffer);
		
		//Send buffer (which holds the repsonse) back to client
		if ((send(fd, buffer, 100,0))== -1) {
			printf("ERROR: Could not send message.\n");
			close(fd);
            break;
		}
	}
	printf("SERVER: Closing client service thread.\n");	
	delete(fd);
	close(fd);	
	return NULL;
}

//This is the thread that prints the bank balances
//It locks the open account mutex so no accounts
//are opened during printing (and so it doesnt print
//while an account is being opened)
void* print_thread(void* arg){
	
	while(1){
		//Pretty straightforward
		printf("SERVER:\nCurrent balances:\n");
		pthread_mutex_lock(&open_account_mutex);
		printBank(bank);
		pthread_mutex_unlock(&open_account_mutex);
		sleep(20);
	}
}

//This signal handler will close all the active clients and
//the server when it receives a SIGINT signal
void sigint_handler(int sig){
	//Close clients
	printf("\nSERVER: Server shutting down. Closing all clients.\n");
	fdnode* temp = head;
	//It traverses the list of currently active clients and sends the "end"
	//command to each. The clients know that an "end" command means to
	//shut down.
	while(temp != NULL){
		int fd = temp->fd;
		if ((send(fd, "end", 3, 0))== -1) {
			printf("ERROR: Could not shut down one of the clients.\n");
			close(fd);
            break;
		}
		temp = temp->next;
	}
	exit(0);
}

//Helper function to determine if a string is entirely whitespace
int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

//This function is what translates commands from the client to actions on the bank
//It stores the response in the passed arg buffer which is sent back to the client
void interact(char* arg1, char* arg2, char* accountname, char buffer[]){
	printf("SERVER: Begin interaction: %s %s\n", arg1, arg2);
	memset(buffer, '\0', strlen(buffer));
	
	//This 'if' statement checks for all valid commands
	if(strcmp(arg1, "open") == 0){
		//Open a new account
		if(is_empty(arg2) != 0){
			sprintf(buffer, "ERROR: Please provide an account name.");
			return;
		}
		
		if(accountname[0] != '\0'){
			sprintf(buffer, "ERROR: Cannot open while in account session.");
			return;	
		}
		
		if(numAccounts == 20){
			sprintf(buffer, "ERROR: Bank is full.");
			return;	
		}
		
		if(getIndex(bank, numAccounts, arg2) != -1){
			sprintf(buffer, "ERROR: That account already exists.");
			return;			
		}
		
		//This lock will ensure that accounts are not opened at the same time
		pthread_mutex_lock(&open_account_mutex);
		account* newAccount = addAccount(bank, numAccounts, arg2);
		numAccounts++;
		sprintf(buffer, "SERVER: Opened account: %s", newAccount->name);
		pthread_mutex_unlock(&open_account_mutex);
		
	} else if(strcmp(arg1, "start") == 0){		
		//Start interaction with an account
		if(accountname[0] != '\0'){
			sprintf(buffer, "ERROR: Please finish current account session.");
			return;			
		}
		if(is_empty(arg2) != 0){
			sprintf(buffer, "ERROR: Please provide an account name.");
			return;
		}
		int index = getIndex(bank, numAccounts, arg2);
		if(index == -1){
			sprintf(buffer, "ERROR: Please provide a valid account.");
			return;			
		}
		
		//This checks if the mutex is locked. If it's not then this thread will
		//lock it and start the account. 
		//Each account has its own mutex in the account_mutexes array.
		//If the mutex has already been locked then that means another client
		//is accessing that account. The user will be notified.
		if(pthread_mutex_trylock(&account_mutexes[index]) != 0){
			sprintf(buffer, "ERROR: This account is already in session elsewhere.");
			return;	
		}
		
		bank[index]->inSessionFlag = 1;
		
		strcpy(accountname, arg2);
		sprintf(buffer, "SERVER: Started account: %s", accountname);
		
	} else if(strcmp(arg1, "credit") == 0){
		//Put money in an account
		if(accountname[0] == '\0'){
			sprintf(buffer, "ERROR: Credit command only valid in account session.");
			return;			
		}
		if(is_empty(arg2) != 0){
			sprintf(buffer, "ERROR: Please provide an amount.");
			return;
		}
		if(credit(bank, numAccounts, atof(arg2), accountname) == -1){
			sprintf(buffer, "ERROR: Invalid credit amount.");
			return;
		}
		sprintf(buffer, "SERVER: Credit successful.");
	} else if(strcmp(arg1, "debit") == 0){
		//Take money out of an account
		if(accountname[0] == '\0'){
			sprintf(buffer, "ERROR: Debit command only valid in account session.");
			return;
		}
		if(is_empty(arg2) != 0){
			sprintf(buffer, "ERROR: Please provide an amount.");
		}
		if(debit(bank, numAccounts, atof(arg2), accountname) == -1){
			sprintf(buffer, "ERROR: Invalid debit amount.");
			return;
		}
		sprintf(buffer, "SERVER: Debit successful.");
	} else if(strcmp(arg1, "balance") == 0){
		//Check the balance of an account
		if(accountname[0] == '\0'){
			sprintf(buffer, "ERROR: Balance command only valid in account session.");
			return;
		}
		float bal;
		bal = balance(bank, numAccounts, accountname);
		sprintf(buffer, "%0.2f", bal);
		
	} else if(strcmp(arg1, "finish") == 0){
		//Finish interaction with an account		
		if(accountname[0] == '\0'){
			sprintf(buffer, "ERROR: Not in account session.");
			return;
		}
		
		//This is one instance where a mutex is unlocked.
		//The account session is finished and so its corresponding mutex is unlocked.
		int index = getIndex(bank, numAccounts, accountname);
		pthread_mutex_unlock(&account_mutexes[index]);
		bank[index]->inSessionFlag = 0;		
		
		sprintf(buffer, "SERVER: Finished account session.");
		accountname[0] = '\0';			
		
	} else if(strcmp(arg1, "exit") == 0){
		//Close the client		
		if(accountname[0] != '\0'){
			//This is the second instance where a mutex is unlocked.
			//If a client exits before finishing a session, we finish the session for the client.
			int index = getIndex(bank, numAccounts, accountname);
			pthread_mutex_unlock(&account_mutexes[index]);	
			bank[index]->inSessionFlag = 0;
		}
		//Send the command which ends the client process.
		sprintf(buffer, "end");		
	} else {
		sprintf(buffer, "ERROR: Command not recognized.");
		return;
	}
}

//FD LINKED LIST METHODS:
//These methods maintain a linked list of all the client FD's so we
//know who we are currently talking to.
//This allows us to end all clients when the server receives
//a SIGINT signal.
fdnode* create_node(int fd){
	fdnode* ret = (fdnode *)malloc(sizeof(fdnode));
	ret->fd = fd;
	ret->next = NULL;
	return ret;
}

fdnode* add(int fd){
	fdnode* new_node = create_node(fd);
	if(head == NULL){
		head = new_node;
		return head;
	}
	fdnode* temp = head->next;
	head->next = new_node;
	new_node->next = temp;
	return new_node;
}

int delete(int fd){
	fdnode* temp = head;
	fdnode* next;
	if(temp != NULL){
		if(temp->fd == fd){
			head = temp->next;
			free(temp);
			return 0;
		}
		next = temp->next;
	} else {
		return 0;
	}
	while(next != NULL){
		if(next->fd == fd){
			temp->next = next->next;
			fdnode* tempnext = next->next;
			free(next);
			next = tempnext;
		}
	}
	return 0;
}
