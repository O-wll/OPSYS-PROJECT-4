#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <time.h>

#define MSG_KEY 864049
#define RANDOM_TERMINATION 5 // Constant for termination of child.

typedef struct ossMSG { // Message structure
	long mtype;
	int msg;
} ossMSG;

int main(int argc, char **argv) { // Main program
	srand(time(NULL));

	int msgid = msgget(MSG_KEY, 0666); // Get message from shared memory
	if (msgid == -1) {
        	printf("Error: User msgget failed. \n");
        	exit(1);
    	}

	// Receive message from OSS
	ossMSG receiveMSG;
	if (msgrcv(msgid, &receiveMSG, sizeof(int), getpid(), 0) == -1) {
        	printf("Error: User msgrcv failed. \n");
        	exit(1);
    	}
}
