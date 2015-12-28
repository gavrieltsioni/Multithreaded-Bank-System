#include <stdio.h>
typedef struct fdnode{
	int fd;
	struct fdnode *next;	
} fdnode;

void* client_acceptor_thread(void* arg);
void* client_service_thread(void* arg);
void interact(char* arg1, char* arg2, char* accountname, char buffer[]);
void* print_thread(void* arg);
void sigint_handler(int sig);
int delete(int fd);
fdnode* add(int fd);
fdnode* create_node(int fd);



