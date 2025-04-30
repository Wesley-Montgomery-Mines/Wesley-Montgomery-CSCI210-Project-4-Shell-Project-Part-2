#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20]; // This is the global variable to store username

// These are the allowed commands
char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod","diff", "cd", "exit", "help", "sendmsg"};

// This is the message structure exchanged via FIFOs
// Structure to represent a message between users
struct message
{
	char source[50]; // This is the sender username
	char target[50]; // This is the recipient username
	char msg[200];	 // This is the message content
};

// This will signal handler to clean up on termination
void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

// The is the function to send a message from this user to another user
void sendmsg(char *user, char *target, char *msg){
	// TODO:
	// Send a request to the server to send the message (msg) to the target user (target)
	// by creating the message structure and writing it to server's FIFO

	// THis is the structure to represent a message between users
	struct message m;
	strcpy(m.source, user);
	strcpy(m.target, target);
	strcpy(m.msg, msg);

	int serverFIFO = open("serverFIFO",O_WRONLY);

	write(serverFIFO, &req, sizeof(req));
}

// This is the thread function to continuously listen for messages sent to this user
void *messageListener(void *arg){
// TODO:
	// Read user's own FIFO in an infinite loop for incoming messages
	// The logic is similar to a server listening to requests
	// print the incoming message to the standard output in the
	// following format
	// Incoming message from [source]: [message]
	// put an end of line at the end of the message

	char* user_name = (char*) arg;

	int fifo_id = open(user_name, O_RDONLY);

	struct message req;

	while (1) {
		ssize_t read_bytes = read(fifo_id, &req, sizeof(req));
		
		if (read_bytes == 0) {
			close(fifo_id);
			fifo_id = open(user_name, O_RDONLY);
			continue;
		}

		printf("Incoming message from %s: %s\n",req.source,req.msg);
	}

	pthread_exit((void*)0);
}

// This checks if a given command is allowed
int isAllowed(const char *cmd)
{
	for (int i = 0; i < N; i++)
	{
		if (strcmp(cmd, allowed[i]) == 0)
			return 1;
	}
	return 0;
}

// This is the main shell interface logic
int main(int argc, char **argv){
	pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}
	
	signal(SIGINT, terminate);
	strcpy(uName, argv[1]);

	// This will create the message listener thread
	pthread_t listener;
	pthread_create(&listener, NULL, messageListener, &uName);

	while (1)
	{
		fprintf(stderr, "rsh> ");

		if (fgets(line,256,stdin)==NULL)
			continue;

		if (strcmp(line, "\n") == 0)
			continue;

		line[strlen(line)-1]='\0';

		char cmd[256];
		char line2[256];
		strcpy(line2,line);
		strcpy(cmd,strtok(line," "));

		// This will parse command and arguments
		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		// This will handle the built in sendmsg command
		if (strcmp(cmd, "sendmsg") == 0){
			// TODO: Create the target user and
			// the message string and call the sendmsg function

			// NOTE: The message itself can contain spaces
			// If the user types: "sendmsg user1 hello there"
			// target should be "user1"
			// and the message should be "hello there"

			// if no argument is specified, you should print the following
			// printf("sendmsg: you have to specify target user\n");
			// if no message is specified, you should print the followingA
 			// printf("sendmsg: you have to enter a message\n");

			char* target = strtok(NULL, " ");
		if (target == NULL) {
			printf("sendmsg: you have to specify target user\n");
		}

		char msg[200] = "";

		char* word = strtok(NULL, " ");

		if (word == NULL) {
			printf("sendmsg: you have to enter a message\n");
		}

		while (word != NULL) {
			strcat(msg, word);
			strcat(msg, " ");
			word = strtok(NULL, " ");
		}
		msg[strlen(msg) - 1] = '\0';
		sendmsg(uName, target, msg);

		continue;
	}

	if (strcmp(cmd,"exit") == 0)
	break;

	if (strcmp(cmd,"cd") == 0) {
		char *targetDir=strtok(NULL," ");
		if (strtok(NULL," ")!=NULL) {
			printf("-rsh: cd: too many arguments\n");
		}
		else {
			chdir(targetDir);
		}
		continue;
	}

	if (strcmp(cmd,"help")==0) {
		printf("The allowed commands are:\n");
		for (int i=0;i<N;i++) {
			printf("%d: %s\n",i+1,allowed[i]);
		}
		continue;
	}

	cargv = (char**)malloc(sizeof(char*));
	cargv[0] = (char *)malloc(strlen(cmd)+1);
	path = (char *)malloc(9+strlen(cmd)+1);
	strcpy(path,cmd);
	strcpy(cargv[0],cmd);

	char *attrToken = strtok(line2," "); // This will skip cargv[0]
	attrToken = strtok(NULL, " ");
	int n = 1;
	while (attrToken!=NULL) {
		n++;
		cargv = (char**)realloc(cargv,sizeof(char*)*n);
		cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
		strcpy(cargv[n-1],attrToken);
		attrToken = strtok(NULL, " ");
	}
	cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
	cargv[n] = NULL;

	// THis will initialize spawn attributes
	posix_spawnattr_init(&attr);

	// This will spawn a new process
	if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
		perror("spawn failed");
		exit(EXIT_FAILURE);
	}

	// WThis will make the program wait for the spawned process to terminate
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid failed");
		exit(EXIT_FAILURE);
	}

	// This will destroy spawn attributes
	posix_spawnattr_destroy(&attr);

    }
    return 0;
}
