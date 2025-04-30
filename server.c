#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// This will define the structure used for inter-process communication
struct message
{
	char source[50]; // This is the sender username
	char target[50]; // This is the target username and FIFO name
	char msg[200];	 // This is the message body
};

// This will terminate the server process on signals like Ctrl + c
void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main()
{
	int server;			// This is the file descriptor for reading from server FIFO
	int target;			// This is the file descriptor for writing to target FIFO
	int dummyfd;		// This is the dummy write-end to keep serverFIFO open
	struct message req; // This is the incoming request structure

	// This will ignore SIGPIPE to avoid crashing if client closes its FIFO unexpectedly
	signal(SIGPIPE, SIG_IGN);

	// This will handle interrupt signals to allow graceful shutdown
	signal(SIGINT, terminate);

	// This will open the server FIFO in read-only mode
	server = open("serverFIFO", O_RDONLY);
	if (server < 0)
	{
		perror("Failed to open serverFIFO for reading");
		return 1;
	}

	// This is the open dummy write-end so reads don't return 0 when no clients are writing
	dummyfd = open("serverFIFO", O_WRONLY);
	if (dummyfd < 0)
	{
		perror("Failed to open dummy write end of serverFIFO");
		close(server);
		return 1;
	}

	// This is the server loop to continuously process incoming requests
	while (1)
	{
		// This will read the full message from the FIFO
		if (read(server, &req, sizeof(req)) <= 0)
		{
			continue; // This will skip if read fails or is incomplete
		}

		// This will print request details for logging and debugging
		printf("Received a request from %s to send the message \"%s\" to %s.\n",
				req.source, req.msg, req.target);

		// This will open the target user's FIFO in write-only mode
		target = open(req.target, O_WRONLY);
		if (target < 0)
		{
			perror("Failed to open target FIFO");
			continue;
		}

		// This will write the message structure to the target's FIFO
		if (write(target, &req, sizeof(req)) < 0)
		{
			perror("Failed to write to target FIFO");
		}

		// This will close the target FIFO
		close(target);
	}

	// This is just the cleanup
	close(server);
	close(dummyfd);
	return 0;
}
