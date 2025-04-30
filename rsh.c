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
char *allowed[N] = {
	"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod",
	"diff", "cd", "exit", "help", "sendmsg"};

// This is the message structure exchanged via FIFOs
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
void sendmsg(char *user, char *target, char *msg)
{
	struct message m;
	strcpy(m.source, user);
	strcpy(m.target, target);
	strcpy(m.msg, msg);

	int fidscrpt = open("serverFIFO", O_WRONLY);
	if (fidscrpt < 0)
	{
		perror("sendmsg: open");
		return;
	}

	write(fidscrpt, &m, sizeof(m));
	close(fidscrpt);
}

// This is the thread function to continuously listen for messages sent to this user
void *messageListener(void *arg)
{
	char fifoName[50];
	snprintf(fifoName, sizeof(fifoName), "%s", uName);

	int fidscrpt;
	struct message m;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, terminate);

	while (1)
	{
		fidscrpt = open(fifoName, O_RDONLY);
		if (fidscrpt < 0)
		{
			perror("messageListener: open");
			sleep(1);
			continue;
		}

		if (read(fidscrpt, &m, sizeof(struct message)) > 0)
		{
			printf("Incoming message from %s: %s\n", m.source, m.msg);
			fflush(stdout);
		}

		close(fidscrpt);
	}

	pthread_exit(NULL);
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
int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}

	strcpy(uName, argv[1]);
	signal(SIGINT, terminate);

	// This will create the message listener thread
	pthread_t msgThreadId;
	pthread_create(&msgThreadId, NULL, messageListener, NULL);

	char line[256];
	while (1)
	{
		fprintf(stderr, "rsh> ");

		if (fgets(line, sizeof(line), stdin) == NULL)
			continue;
		if (strcmp(line, "\n") == 0)
			continue;
		line[strcspn(line, "\n")] = '\0';

		// This will parse command and arguments
		char *cmd = strtok(line, " ");
		if (!cmd)
			continue;

		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		// This will handle the built in sendmsg command
		if (strcmp(cmd, "sendmsg") == 0)
		{
			char *target = strtok(NULL, " ");
			char *msgBody = strtok(NULL, "");

			if (!target)
			{
				printf("sendmsg: you have to specify a target\n");
				continue;
			}
			if (!msgBody)
			{
				printf("sendmsg: you have to enter a message\n");
				continue;
			}

			sendmsg(uName, target, msgBody);
			continue;
		}

		// This will handle the exit command
		if (strcmp(cmd, "exit") == 0)
			break;

		// This will handle the cd command
		if (strcmp(cmd, "cd") == 0)
		{
			char *dir = strtok(NULL, " ");
			if (strtok(NULL, " "))
			{
				printf("cd: too many arguments\n");
			}
			else
			{
				chdir(dir);
			}
			continue;
		}

		// This will handle the help command
		if (strcmp(cmd, "help") == 0)
		{
			printf("Allowed commands:\n");
			for (int i = 0; i < N; i++)
			{
				printf("%s\n", allowed[i]);
			}
			continue;
		}

		// This will spawn external allowed command
		char *args[20];
		args[0] = cmd;
		int argCount = 1;
		char *token;
		while ((token = strtok(NULL, " ")) != NULL)
		{
			args[argCount++] = token;
		}
		args[argCount] = NULL;

		posix_spawnattr_t attr;
		posix_spawnattr_init(&attr);
		pid_t pid;

		if (posix_spawnp(&pid, cmd, NULL, &attr, args, environ) != 0)
		{
			perror("posix_spawnp failed");
			continue;
		}

		int status;
		waitpid(pid, &status, 0);
		posix_spawnattr_destroy(&attr);
	}

	return 0;
}
