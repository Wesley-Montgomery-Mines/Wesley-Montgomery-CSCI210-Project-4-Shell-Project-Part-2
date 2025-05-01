#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Message struct for inter-user communication
struct message {
    char source[50];
    char target[50];
    char msg[200];
};

// Signal handler to exit cleanly
void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server;      // FIFO descriptor for reading messages
    int target;      // Target FIFO descriptor
    int dummyfd;     // Dummy descriptor to prevent EOF
    struct message req;

    signal(SIGPIPE, SIG_IGN);    // Ignore SIGPIPE
    signal(SIGINT, terminate);   // Handle Ctrl+C

    // Open server FIFO for reading and writing
    server = open("serverFIFO", O_RDONLY);
    dummyfd = open("serverFIFO", O_WRONLY);  // Keeps FIFO open

    while (1) {
		// TODO:
		// read requests from serverFIFO
        if (read(server, &req, sizeof(struct message)) != sizeof(struct message)) {
            continue;
        }

        // Print the received request
        printf("Received a request from %s to send the message %s to %s.\n",req.source, req.msg, req.target);
		
		// TODO:
		// open target FIFO and write the whole message struct to the target FIFO
		// close target FIFO after writing the message
        // Open target user's FIFO and send the message
        target = open(req.target, O_WRONLY);
        if (target >= 0) {
            write(target, &req, sizeof(struct message));
            close(target);
        }
    }

    // Cleanup
    close(server);
    close(dummyfd);
    return 0;
}
