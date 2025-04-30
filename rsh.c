#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

// This is the total number of allowed commands including sendmsg
#define N 13

extern char **environ; // This is for posix_spawn
char uName[20];		   // This stores the current user's name

// This is the list of allowed commands
char *allowed[N] = {
	"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod",
	"diff", "cd", "exit", "help", "sendmsg"};

// This is for the structure for inter-process messages
struct message
{
	char source[50]; // This is the sender's username
	char target[50]; // This is the recipient's username
	char msg[200];	 // This is the message body
};

// This is for signal handler for CTRL + C or termination
void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

// This sends a message to the server FIFO for forwarding to the target
void sendmsg(char *user, char *target, char *msg)
{
	struct message msgStructure;
	strcpy(msgStructure.target, target);
	strcpy(msgStructure.msg, msg);
	strcpy(msgStructure.source, user);

	int writer = open("serverFIFO", O_WRONLY);
	if (writer != -1)
	{
		write(writer, &msgStructure, sizeof(struct message));
		close(writer);
	}
}

// This is the thread that listens to this user's FIFO and prints incoming messages
void *messageListener(void *arg)
{
	int userFd;
	struct message userRead;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, terminate);

	while (1)
	{
		userFd = open(uName, O_RDONLY);
		if (userFd == -1)
		{
			perror("Failed to open user FIFO");
			sleep(1);
			continue;
		}

		if (read(userFd, &userRead, sizeof(struct message)) > 0)
		{
			printf("Incoming message from %s: %s\n", userRead.source, userRead.msg);
			fflush(stdout);
		}
		close(userFd);
	}
	pthread_exit(NULL);
}

// This will return 1 if the command is allowed or else 0
int isAllowed(const char *cmd)
{
	for (int i = 0; i < N; i++)
	{
		if (strcmp(cmd, allowed[i]) == 0)
			return 1;
	}
	return 0;
}

// This is the main loop for rsh shell
int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}

	// This will store the username
	strcpy(uName, argv[1]);
	signal(SIGINT, terminate);

	// This will start the listener thread
	pthread_t msgThreadId;
	pthread_create(&msgThreadId, NULL, messageListener, NULL);

	while (1)
	{
		fprintf(stderr, "rsh>");
		char line[256], line2[256], cmd[256];

		if (fgets(line, sizeof(line), stdin) == NULL)
			continue;
		if (strcmp(line, "\n") == 0)
			continue;

		line[strlen(line) - 1] = '\0';
		strcpy(line2, line);
		strcpy(cmd, strtok(line, " "));

		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		// This will handle the special sendmsg command
		if (strcmp(cmd, "sendmsg") == 0)
		{
			char *args[20] = {0};
			char *token = strtok(line2, " ");
			int argCount = 0;
			while (token != NULL && argCount < 19)
			{
				args[argCount++] = token;
				token = strtok(NULL, " ");
			}
			args[argCount] = NULL;

			if (!args[1])
			{
				printf("sendmsg: you have to specify a target\n");
				continue;
			}
			if (!args[2])
			{
				printf("sendmsg: you have to enter a message\n");
				continue;
			}

			// This will reconstruct message string from args[2..]
			char *message = (char *)malloc(256);
			message[0] = '\0';
			for (int i = 2; i < argCount; i++)
			{
				strcat(message, args[i]);
				if (i < argCount - 1)
					strcat(message, " ");
			}

			sendmsg(argv[1], args[1], message);
			free(message);
			continue;
		}

		// This will handle the exit command
		if (strcmp(cmd, "exit") == 0)
			break;

		// This will handle the cd command internally
		if (strcmp(cmd, "cd") == 0)
		{
			char *targetDir = strtok(NULL, " ");
			if (strtok(NULL, " ") != NULL)
				printf("-rsh: cd: too many arguments\n");
			else
				chdir(targetDir);
			continue;
		}

		// This will handle the help command
		if (strcmp(cmd, "help") == 0)
		{
			printf("The allowed commands are:\n");
			for (int i = 0; i < N; i++)
				printf("%d: %s\n", i + 1, allowed[i]);
			continue;
		}

		// This will handle allowed external commands using posix_spawn
		char *path = strdup(cmd);
		char **cargv = malloc(sizeof(char *) * 2);
		cargv[0] = strdup(cmd);

		int n = 1;
		char *arg = strtok(line2, " ");
		arg = strtok(NULL, " ");
		while (arg != NULL)
		{
			cargv = realloc(cargv, sizeof(char *) * (n + 2));
			cargv[n++] = strdup(arg);
			arg = strtok(NULL, " ");
		}
		cargv[n] = NULL;

		posix_spawnattr_t attr;
		posix_spawnattr_init(&attr);
		pid_t pid;

		if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0)
		{
			perror("spawn failed");
			exit(EXIT_FAILURE);
		}

		int status;
		waitpid(pid, &status, 0);
		posix_spawnattr_destroy(&attr);

		free(path);
		for (int i = 0; i < n; i++)
			free(cargv[i]);
		free(cargv);
	}

	return 0;
}