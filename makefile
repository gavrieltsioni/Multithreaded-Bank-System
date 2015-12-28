ALL:
	gcc -Wall -g -o server server.c account.c -lpthread; gcc -Wall -g -o client client.c -lpthread
clean:
	rm server; rm client
