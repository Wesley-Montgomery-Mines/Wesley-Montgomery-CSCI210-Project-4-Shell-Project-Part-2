#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// This will define the structure of a message exchanged between users
struct message
{
	char source[50]; // This is the username of the sender
	char target[50]; // This is the username of the recipient (FIFO name)
	char msg[200];	 // This is the the message content
};

// This will terminate the server process when it gets the signal
void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main()
{
	int server, target, dummyfd;
	struct message req;

	// This will ignore broken pipe signals
	signal(SIGPIPE, SIG_IGN);

	// This will register handler to clean up on Ctrl + C
	signal(SIGINT, terminate);

	// This will open server FIFO for reading messages sent from clients
	server = open("serverFIFO", O_RDONLY);
	if (server == -1)
	{
		perror("Failed to open serverFIFO for reading");
		exit(EXIT_FAILURE);
	}

	// This will open dummy write end to keep the FIFO open when no clients are writing
	dummyfd = open("serverFIFO", O_WRONLY);
	if (dummyfd == -1)
	{
		perror("Failed to open dummy write-end of serverFIFO");
		close(server);
		exit(EXIT_FAILURE);
	}

	// This will continuously process incoming messages from clients
	while (1)
	{
		// This will read a full message structure from the FIFO
		if (read(server, &req, sizeof(struct message)) != sizeof(struct message))
		{
			continue; // This will skip the iteration if there is an incomplete or bad read
		}

		// This will display info for logging and debugging
		printf("Received a request from %s to send the message \"%s\" to %s.\n",
			req.source, req.msg, req.target);

		// This will check if the recipient's FIFO exists before writing to it
		if (access(req.target, F_OK) != 0)
		{
			fprintf(stderr, "Target FIFO %s not found. Skipping.\n", req.target);
			continue;
		}

		// This will open recipient FIFO and send the message structure
		target = open(req.target, O_WRONLY);
		if (target != -1)
		{
			write(target, &req, sizeof(struct message));
			close(target);
		}
		else
		{
			perror("Failed to open target FIFO");
		}
	}

	// This will clean up FIFO file descriptors
	close(server);
	close(dummyfd);
	return 0;
}