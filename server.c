#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// This will define the structure used for inter-process communication
struct message{
	char source[50]; // This is the sender username
	char target[50]; // This is the target username and FIFO name
	char msg[200];	 // This is the message body
};

// This will terminate the server process on signals like Ctrl + c
void terminate(int sig){
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

// Entry point for the server process
int main(){
	int server;			// This is the file descriptor for reading from server FIFO
	int target;			// This is the file descriptor for writing to target FIFO
	int dummyfd;		// This is the dummy write-end to keep serverFIFO open
	struct message req; // This is the incoming request structure

	// This will ignore SIGPIPE to avoid crashing if client closes its FIFO unexpectedly
	signal(SIGPIPE, SIG_IGN);

	// This will handle interrupt signals to allow graceful shutdown
	signal(SIGINT, terminate);
	server = open("serverFIFO",O_RDONLY);
	dummyfd = open("serverFIFO",O_WRONLY);

	// This is the server loop to continuously process incoming requests
	while (1){
		// This will read the full message from the FIFO
		ssize_t read_bytes = read(server, &req, sizeof(req));
		(void)read_bytes;

		// This will print request details for logging and debugging
		printf("Received a request from %s to send the message \"%s\" to %s.\n",req.source, req.msg, req.target);

		target = open(req.target, O_WRONLY);
		write(target, &req, sizeof(req));
		close(target);
	}

	// This is just the cleanup
	close(server);
	close(dummyfd);
	return 0;
}
